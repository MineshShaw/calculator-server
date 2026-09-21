#include "http_parser.hpp"

#include <gtest/gtest.h>

namespace {

std::string req(const std::string& request_line, const std::string& extra_headers = "Host: localhost:8080\r\n") {
  return request_line + "\r\n" + extra_headers + "\r\n";
}

}  // namespace

TEST(HttpParser, ExtractsMethodPathQueryAndHeaders) {
  const auto parsed = HttpParser::parse_one(req("GET /add?a=2&b=3 HTTP/1.1"));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->method, "GET");
  EXPECT_EQ(parsed->path, "/add");
  EXPECT_EQ(parsed->query, "a=2&b=3");
  EXPECT_EQ(parsed->version, "HTTP/1.1");
  EXPECT_EQ(parsed->query_params.at("a"), "2");
  EXPECT_EQ(parsed->query_params.at("b"), "3");
  EXPECT_EQ(HttpParser::header_value(*parsed, "Host"), "localhost:8080");
  EXPECT_EQ(HttpParser::header_value(*parsed, "host"), "localhost:8080");
}

TEST(HttpParser, IncrementalFeedUntilCrlfCrlf) {
  HttpParser parser;
  EXPECT_EQ(parser.feed("GET /sub?a=10&b=4 HTTP/1.1\r\nHost: localhost"),
            HttpParser::Status::NeedMore);
  EXPECT_FALSE(parser.has_complete_request());
  EXPECT_EQ(parser.feed("\r\n\r\n"), HttpParser::Status::Complete);
  ASSERT_TRUE(parser.has_complete_request());
  const HttpRequest r = parser.take_request();
  EXPECT_EQ(r.path, "/sub");
  EXPECT_EQ(r.query_params.at("a"), "10");
  EXPECT_EQ(r.query_params.at("b"), "4");
}

TEST(HttpParser, HeaderNameIsCaseInsensitive) {
  const auto parsed =
      HttpParser::parse_one(req("GET /mul?a=6&b=7 HTTP/1.1", "HOST: Example.COM\r\n"));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(HttpParser::header_value(*parsed, "host"), "Example.COM");
}

TEST(HttpParser, MissingQuery) {
  const auto parsed = HttpParser::parse_one(req("GET /add HTTP/1.1"));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->path, "/add");
  EXPECT_TRUE(parsed->query.empty());
  EXPECT_TRUE(parsed->query_params.empty());
}

TEST(HttpParser, PostWithBodyAndContentLength) {
  const std::string raw =
      "POST /add HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "abcde";
  const auto parsed = HttpParser::parse_one(raw);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->method, "POST");
  EXPECT_EQ(parsed->body, "abcde");
}

TEST(HttpParser, PipelinedRequestsLeaveLeftoverBytes) {
  const std::string raw = req("GET /add?a=1&b=1 HTTP/1.1") + req("GET /sub?a=5&b=2 HTTP/1.1");
  std::string leftover;
  const auto first = HttpParser::parse_one(raw, &leftover);
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->path, "/add");
  EXPECT_FALSE(leftover.empty());

  const auto second = HttpParser::parse_one(leftover);
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->path, "/sub");
  EXPECT_EQ(second->query_params.at("a"), "5");
}

TEST(HttpParser, MalformedRequestLineIsError) {
  HttpParser parser;
  EXPECT_EQ(parser.feed("NOTHTTP\r\n\r\n"), HttpParser::Status::Error);
  EXPECT_TRUE(parser.failed());
}

TEST(HttpParser, IncompleteIsNeedMore) {
  HttpParser parser;
  EXPECT_EQ(parser.feed("GET /add HTTP/1.1\r\nHost: localhost\r\n"),
            HttpParser::Status::NeedMore);
  EXPECT_FALSE(parser.has_complete_request());
}
