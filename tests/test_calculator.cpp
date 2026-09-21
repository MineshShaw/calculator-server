#include "calculator.hpp"

#include <gtest/gtest.h>

#include <limits>

TEST(Calculator, Add) {
  EXPECT_DOUBLE_EQ(Calculator::add(2, 3), 5.0);
  EXPECT_DOUBLE_EQ(Calculator::add(-1.5, 0.5), -1.0);
  EXPECT_DOUBLE_EQ(Calculator::add(0, 0), 0.0);
}

TEST(Calculator, Sub) {
  EXPECT_DOUBLE_EQ(Calculator::sub(10, 4), 6.0);
  EXPECT_DOUBLE_EQ(Calculator::sub(1, 4), -3.0);
}

TEST(Calculator, Mul) {
  EXPECT_DOUBLE_EQ(Calculator::mul(6, 7), 42.0);
  EXPECT_DOUBLE_EQ(Calculator::mul(-2, 3), -6.0);
  EXPECT_DOUBLE_EQ(Calculator::mul(0, 99), 0.0);
}

TEST(Calculator, Div) {
  const auto q = Calculator::div(9, 3);
  ASSERT_TRUE(q.has_value());
  EXPECT_DOUBLE_EQ(*q, 3.0);

  const auto q2 = Calculator::div(1, 4);
  ASSERT_TRUE(q2.has_value());
  EXPECT_DOUBLE_EQ(*q2, 0.25);
}

TEST(Calculator, DivideByZero) {
  EXPECT_FALSE(Calculator::div(1, 0).has_value());
  EXPECT_FALSE(Calculator::div(0, 0).has_value());
  EXPECT_FALSE(Calculator::div(-5, 0.0).has_value());
}

TEST(Calculator, ParseNumberValid) {
  EXPECT_DOUBLE_EQ(*Calculator::parse_number("2"), 2.0);
  EXPECT_DOUBLE_EQ(*Calculator::parse_number("  3.5 "), 3.5);
  EXPECT_DOUBLE_EQ(*Calculator::parse_number("-10"), -10.0);
  EXPECT_DOUBLE_EQ(*Calculator::parse_number("+8"), 8.0);
}

TEST(Calculator, ParseNumberInvalid) {
  EXPECT_FALSE(Calculator::parse_number("").has_value());
  EXPECT_FALSE(Calculator::parse_number("x").has_value());
  EXPECT_FALSE(Calculator::parse_number("2abc").has_value());
  EXPECT_FALSE(Calculator::parse_number("abc2").has_value());
  EXPECT_FALSE(Calculator::parse_number("   ").has_value());
  EXPECT_FALSE(Calculator::parse_number("inf").has_value());
  EXPECT_FALSE(Calculator::parse_number("nan").has_value());
}

TEST(Calculator, FormatNumber) {
  EXPECT_EQ(Calculator::format_number(5.0), "5");
  EXPECT_EQ(Calculator::format_number(0.0), "0");
  EXPECT_EQ(Calculator::format_number(-3.0), "-3");
  EXPECT_NE(Calculator::format_number(2.5).find('2'), std::string::npos);
}

TEST(Calculator, ArithmeticEdge) {
  EXPECT_DOUBLE_EQ(Calculator::add(std::numeric_limits<double>::min(), 0),
                   std::numeric_limits<double>::min());
  EXPECT_DOUBLE_EQ(Calculator::mul(1e10, 2), 2e10);
}
