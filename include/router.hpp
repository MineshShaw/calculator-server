#pragma once

#include <string>

#include "http_parser.hpp"

class Router {
 public:
  static std::string handle(const HttpRequest& req);

 private:
  static std::string response(int status, const std::string& reason,
                              const std::string& body,
                              bool keep_alive);
  static bool wants_close(const HttpRequest& req);
};
