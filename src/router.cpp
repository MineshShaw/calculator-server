#include "router.hpp"

#include "calculator.hpp"

#include <sstream>
#include <unordered_set>
#include <utility>

namespace {

const std::unordered_set<std::string> kOps = {"/add", "/sub", "/mul", "/div"};

}  // namespace

bool Router::wants_close(const HttpRequest& req) {
  const std::string conn = HttpParser::header_value(req, "connection");
  return conn == "close";
}

std::string Router::response(int status, const std::string& reason,
                             const std::string& body, bool keep_alive) {
  std::ostringstream oss;
  oss << "HTTP/1.1 " << status << " " << reason << "\r\n";
  oss << "Content-Type: text/plain; charset=utf-8\r\n";
  oss << "Content-Length: " << body.size() << "\r\n";
  oss << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
  oss << "\r\n";
  oss << body;
  return oss.str();
}

std::string Router::handle(const HttpRequest& req) {
  const bool keep_alive = !wants_close(req);

  if (HttpParser::header_value(req, "host").empty()) {
    return response(400, "Bad Request", "", keep_alive);
  }

  if (kOps.count(req.path) == 0) {
    return response(404, "Not Found", "", keep_alive);
  }

  if (req.method != "GET") {
    return response(405, "Method Not Allowed", "", keep_alive);
  }

  const auto a_it = req.query_params.find("a");
  const auto b_it = req.query_params.find("b");
  if (a_it == req.query_params.end() || b_it == req.query_params.end()) {
    return response(400, "Bad Request", "", keep_alive);
  }

  const auto a = Calculator::parse_number(a_it->second);
  const auto b = Calculator::parse_number(b_it->second);
  if (!a || !b) {
    return response(400, "Bad Request", "", keep_alive);
  }

  double result = 0.0;
  if (req.path == "/add") {
    result = Calculator::add(*a, *b);
  } else if (req.path == "/sub") {
    result = Calculator::sub(*a, *b);
  } else if (req.path == "/mul") {
    result = Calculator::mul(*a, *b);
  } else if (req.path == "/div") {
    const auto quot = Calculator::div(*a, *b);
    if (!quot) {
      return response(400, "Bad Request", "", keep_alive);
    }
    result = *quot;
  } else {
    return response(400, "Bad Request", "", keep_alive);
  }

  return response(200, "OK", Calculator::format_number(result), keep_alive);
}
