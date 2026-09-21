#include "http_parser.hpp"
#include "router.hpp"

#include <gtest/gtest.h>

namespace {

HttpRequest make_get(const std::string& path, const std::string& query,
                     bool include_host = true) {
  HttpRequest req;
  req.method = "GET";
  req.path = path;
  req.query = query;
  req.version = "HTTP/1.1";
  if (!query.empty()) {
    std::string leftover;
    const auto parsed = HttpParser::parse_one(
        "GET " + path + "?" + query +
        " HTTP/1.1\r\nHost: localhost:8080\r\n\r\n");
    if (parsed) {
      return *parsed;
    }
  } else {
    std::string raw = "GET " + path + " HTTP/1.1\r\n";
    if (include_host) {
      raw += "Host: localhost:8080\r\n";
    }
    raw += "\r\n";
    const auto parsed = HttpParser::parse_one(raw);
    if (parsed) {
      auto r = *parsed;
      if (!include_host) {
        r.headers.erase("host");
      }
      return r;
    }
  }
  if (include_host) {
    req.headers["host"] = "localhost:8080";
  }
  return req;
}

int status_of(const std::string& resp) {
  if (resp.compare(0, 9, "HTTP/1.1 ") != 0) {
    return -1;
  }
  return std::stoi(resp.substr(9, 3));
}

std::string body_of(const std::string& resp) {
  const auto pos = resp.find("\r\n\r\n");
  if (pos == std::string::npos) {
    return {};
  }
  return resp.substr(pos + 4);
}

}  // namespace

TEST(Router, AddOk) {
  const std::string resp = Router::handle(make_get("/add", "a=2&b=3"));
  EXPECT_EQ(status_of(resp), 200);
  EXPECT_EQ(body_of(resp), "5");
}

TEST(Router, SubOk) {
  const std::string resp = Router::handle(make_get("/sub", "a=10&b=4"));
  EXPECT_EQ(status_of(resp), 200);
  EXPECT_EQ(body_of(resp), "6");
}

TEST(Router, MulOk) {
  const std::string resp = Router::handle(make_get("/mul", "a=6&b=7"));
  EXPECT_EQ(status_of(resp), 200);
  EXPECT_EQ(body_of(resp), "42");
}

TEST(Router, DivOk) {
  const std::string resp = Router::handle(make_get("/div", "a=9&b=3"));
  EXPECT_EQ(status_of(resp), 200);
  EXPECT_EQ(body_of(resp), "3");
}

TEST(Router, DivideByZeroBadRequest) {
  const std::string resp = Router::handle(make_get("/div", "a=1&b=0"));
  EXPECT_EQ(status_of(resp), 400);
}

TEST(Router, InvalidOperandBadRequest) {
  const std::string resp = Router::handle(make_get("/add", "a=x&b=3"));
  EXPECT_EQ(status_of(resp), 400);
}

TEST(Router, UnknownPathNotFound) {
  const std::string resp = Router::handle(make_get("/pow", "a=2&b=8"));
  EXPECT_EQ(status_of(resp), 404);
}

TEST(Router, PostMethodNotAllowed) {
  HttpRequest req;
  req.method = "POST";
  req.path = "/add";
  req.headers["host"] = "localhost:8080";
  const std::string resp = Router::handle(req);
  EXPECT_EQ(status_of(resp), 405);
}

TEST(Router, MissingHostBadRequest) {
  HttpRequest req = make_get("/add", "", false);
  req.headers.erase("host");
  req.query_params["a"] = "1";
  req.query_params["b"] = "2";
  const std::string resp = Router::handle(req);
  EXPECT_EQ(status_of(resp), 400);
}

TEST(Router, MissingOperandsBadRequest) {
  const std::string resp = Router::handle(make_get("/add", ""));
  EXPECT_EQ(status_of(resp), 400);
}

TEST(Router, KeepAliveByDefault) {
  const std::string resp = Router::handle(make_get("/add", "a=1&b=1"));
  EXPECT_NE(resp.find("Connection: keep-alive"), std::string::npos);
}

TEST(Router, ConnectionCloseHonored) {
  HttpRequest req = make_get("/add", "a=1&b=1");
  req.headers["connection"] = "close";
  const std::string resp = Router::handle(req);
  EXPECT_NE(resp.find("Connection: close"), std::string::npos);
}
