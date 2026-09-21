#include "calculator.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>

namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
    s.remove_suffix(1);
  }
  return s;
}

}  // namespace

double Calculator::add(double a, double b) { return a + b; }
double Calculator::sub(double a, double b) { return a - b; }
double Calculator::mul(double a, double b) { return a * b; }

std::optional<double> Calculator::div(double a, double b) {
  if (b == 0.0) {
    return std::nullopt;
  }
  return a / b;
}

std::optional<double> Calculator::parse_number(std::string_view text) {
  const std::string_view trimmed = trim(text);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  std::string copy(trimmed);
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() || *end != '\0' || errno == ERANGE) {
    return std::nullopt;
  }
  if (!std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

std::string Calculator::format_number(double value) {
  if (!std::isfinite(value)) {
    return "nan";
  }

  double integral = 0.0;
  const double fractional = std::modf(value, &integral);
  if (fractional == 0.0 && integral >= static_cast<double>(std::numeric_limits<long long>::min()) &&
      integral <= static_cast<double>(std::numeric_limits<long long>::max())) {
    return std::to_string(static_cast<long long>(integral));
  }

  std::ostringstream oss;
  oss.precision(15);
  oss << value;
  return oss.str();
}
