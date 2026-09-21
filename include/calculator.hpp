#pragma once

#include <optional>
#include <string>
#include <string_view>

class Calculator {
 public:
  static double add(double a, double b);
  static double sub(double a, double b);
  static double mul(double a, double b);

  // Returns std::nullopt when b == 0.
  static std::optional<double> div(double a, double b);

  // Strict numeric parse: rejects empty strings, trailing junk, NaN, and inf.
  static std::optional<double> parse_number(std::string_view text);

  // Formats whole values without a decimal point (e.g. 5, not 5.000000).
  static std::string format_number(double value);
};
