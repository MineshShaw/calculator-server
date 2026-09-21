#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

struct HttpRequest {
  std::string method;
  std::string target;
  std::string path;
  std::string query;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::unordered_map<std::string, std::string> query_params;
  std::string body;
};

class HttpParser {
 public:
  enum class Status { NeedMore, Complete, Error };

  static constexpr std::size_t kMaxHeaderBytes = 65536;
  static constexpr std::size_t kMaxBodyBytes = 1048576;

  Status feed(const char* data, std::size_t len);
  Status feed(std::string_view data);

  bool has_complete_request() const;
  bool failed() const;
  const std::string& error_message() const;

  HttpRequest take_request();

  const std::string& buffered() const { return buffer_; }

  // Parse exactly one request from a mock byte string. Leftover bytes
  // (pipelined data) are written to leftover when it is non-null.
  static std::optional<HttpRequest> parse_one(std::string_view raw,
                                              std::string* leftover = nullptr);

  static std::string header_value(const HttpRequest& req, std::string_view name);

 private:
  Status try_parse();
  bool parse_headers(std::string_view header_block, HttpRequest& out,
                     std::string& err) const;
  static std::string to_lower(std::string_view s);
  static void parse_query(HttpRequest& req);

  std::string buffer_;
  std::optional<HttpRequest> pending_;
  bool failed_ = false;
  std::string error_;
};
