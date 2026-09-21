#include "http_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace {

std::string_view trim_view(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
    s.remove_suffix(1);
  }
  return s;
}

}  // namespace

HttpParser::Status HttpParser::feed(const char* data, std::size_t len) {
  return feed(std::string_view(data, len));
}

HttpParser::Status HttpParser::feed(std::string_view data) {
  if (failed_) {
    return Status::Error;
  }
  if (pending_) {
    buffer_.append(data);
    return Status::Complete;
  }
  buffer_.append(data);
  return try_parse();
}

bool HttpParser::has_complete_request() const { return pending_.has_value(); }

bool HttpParser::failed() const { return failed_; }

const std::string& HttpParser::error_message() const { return error_; }

HttpRequest HttpParser::take_request() {
  HttpRequest req = std::move(*pending_);
  pending_.reset();
  if (!failed_ && !buffer_.empty()) {
    try_parse();
  }
  return req;
}

std::optional<HttpRequest> HttpParser::parse_one(std::string_view raw,
                                                 std::string* leftover) {
  HttpParser parser;
  const Status st = parser.feed(raw);
  if (st != Status::Complete || !parser.pending_) {
    return std::nullopt;
  }
  HttpRequest req = std::move(*parser.pending_);
  parser.pending_.reset();
  if (leftover) {
    *leftover = parser.buffer_;
  }
  return req;
}

std::string HttpParser::header_value(const HttpRequest& req, std::string_view name) {
  const std::string key = to_lower(name);
  const auto it = req.headers.find(key);
  if (it == req.headers.end()) {
    return {};
  }
  return it->second;
}

HttpParser::Status HttpParser::try_parse() {
  if (failed_) {
    return Status::Error;
  }
  if (pending_) {
    return Status::Complete;
  }

  const auto header_end = buffer_.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    if (buffer_.size() > kMaxHeaderBytes) {
      failed_ = true;
      error_ = "headers too large";
      return Status::Error;
    }
    return Status::NeedMore;
  }
  if (header_end > kMaxHeaderBytes) {
    failed_ = true;
    error_ = "headers too large";
    return Status::Error;
  }

  HttpRequest req;
  std::string err;
  if (!parse_headers(std::string_view(buffer_).substr(0, header_end), req, err)) {
    failed_ = true;
    error_ = err;
    return Status::Error;
  }

  const std::string cl = header_value(req, "content-length");
  std::size_t body_len = 0;
  if (!cl.empty()) {
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(cl.c_str(), &end, 10);
    if (end == cl.c_str() || *end != '\0' || errno == ERANGE || parsed < 0) {
      failed_ = true;
      error_ = "invalid Content-Length";
      return Status::Error;
    }
    body_len = static_cast<std::size_t>(parsed);
    if (body_len > kMaxBodyBytes) {
      failed_ = true;
      error_ = "body too large";
      return Status::Error;
    }
  }

  const std::size_t total = header_end + 4 + body_len;
  if (buffer_.size() < total) {
    return Status::NeedMore;
  }

  req.body = buffer_.substr(header_end + 4, body_len);
  buffer_.erase(0, total);
  pending_ = std::move(req);
  return Status::Complete;
}

bool HttpParser::parse_headers(std::string_view header_block, HttpRequest& out,
                               std::string& err) const {
  const auto line_end = header_block.find("\r\n");
  const std::string_view request_line =
      line_end == std::string_view::npos ? header_block : header_block.substr(0, line_end);

  std::size_t sp1 = request_line.find(' ');
  if (sp1 == std::string_view::npos) {
    err = "malformed request line";
    return false;
  }
  std::size_t sp2 = request_line.find(' ', sp1 + 1);
  if (sp2 == std::string_view::npos) {
    err = "malformed request line";
    return false;
  }
  if (request_line.find(' ', sp2 + 1) != std::string_view::npos) {
    err = "malformed request line";
    return false;
  }

  out.method = std::string(request_line.substr(0, sp1));
  out.target = std::string(request_line.substr(sp1 + 1, sp2 - sp1 - 1));
  out.version = std::string(request_line.substr(sp2 + 1));
  if (out.method.empty() || out.target.empty() || out.version.empty()) {
    err = "malformed request line";
    return false;
  }

  const auto qpos = out.target.find('?');
  if (qpos == std::string::npos) {
    out.path = out.target;
    out.query.clear();
  } else {
    out.path = out.target.substr(0, qpos);
    out.query = out.target.substr(qpos + 1);
  }
  parse_query(out);

  if (line_end == std::string_view::npos) {
    return true;
  }

  std::size_t pos = line_end + 2;
  while (pos < header_block.size()) {
    auto next = header_block.find("\r\n", pos);
    std::string_view line = header_block.substr(
        pos, next == std::string_view::npos ? std::string_view::npos : next - pos);
    if (line.empty()) {
      break;
    }
    const auto colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0) {
      err = "malformed header";
      return false;
    }
    std::string name = to_lower(trim_view(line.substr(0, colon)));
    std::string value(trim_view(line.substr(colon + 1)));
    out.headers[std::move(name)] = std::move(value);
    if (next == std::string_view::npos) {
      break;
    }
    pos = next + 2;
  }
  return true;
}

std::string HttpParser::to_lower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

void HttpParser::parse_query(HttpRequest& req) {
  if (req.query.empty()) {
    return;
  }
  std::size_t start = 0;
  while (start <= req.query.size()) {
    const auto amp = req.query.find('&', start);
    const std::string_view pair =
        std::string_view(req.query).substr(start, amp == std::string::npos
                                                     ? std::string::npos
                                                     : amp - start);
    if (!pair.empty()) {
      const auto eq = pair.find('=');
      if (eq == std::string_view::npos) {
        req.query_params.emplace(std::string(pair), "");
      } else {
        req.query_params[std::string(pair.substr(0, eq))] =
            std::string(pair.substr(eq + 1));
      }
    }
    if (amp == std::string::npos) {
      break;
    }
    start = amp + 1;
  }
}
