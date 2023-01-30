/**
 * \file
 * \brief  Class for formatting values to pretty-printable strings
 */

#ifndef INCLUDE_UTIL_PREFIX_FORMATTER_H_
#define INCLUDE_UTIL_PREFIX_FORMATTER_H_

#include <math.h>

#include <array>
#include <cmath>
#include <sstream>
#include <tuple>

namespace util {

using Prefix = std::pair<int, std::string_view>;
using PrefixArray = std::array<Prefix, 4>;

static constexpr PrefixArray kPrefixes{{
    {0, ""},
    {3, "k"},
    {6, "M"},
    {9, "G"},
}};

/**
 * @brief Format value as string using metric prefix (from International System of Units)
 *
 * @tparam T Value type
 * @param value Value
 * @param unit Custom unit to concatenate on string
 * @return Formatted string
 */
template <typename T>
std::string format_with_prefix(T value, std::string unit) {
  std::ostringstream ss;

  if (value == 0) {
    ss << "0 " << unit;
    return std::move(ss).str();
  }

  float base = log(value) / log(10);

  PrefixArray::const_reverse_iterator rit;
  for (rit = kPrefixes.rbegin(); rit < kPrefixes.rend(); ++rit) {
    if (base >= rit->first) break;
  }

  ss << (value / std::pow(10, rit->first)) << " " << rit->second << unit;
  return std::move(ss).str();
}

/**
 * @brief Format numeric value as string with given precision for decimal values
 * @tparam T Value type
 * @param value Value
 * @param n Precision
 * @return Formatted string
 */
template <typename T>
std::string to_string_with_precision(const T value, const int n = 6) {
  std::ostringstream ss;
  ss.precision(n);
  ss << std::fixed << value;
  return std::move(ss).str();
}

}  // namespace util
#endif  // INCLUDE_UTIL_PREFIX_FORMATTER_H_