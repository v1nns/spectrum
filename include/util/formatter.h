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

#include "ftxui/component/event.hpp"  // for Event

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
std::string format_with_prefix(const T& value, const std::string& unit) {
  std::ostringstream ss;

  if (value == 0) {
    ss << "0 " << unit;
    return std::move(ss).str();
  }

  float base = double(log(value) / log(10));

  PrefixArray::const_reverse_iterator rit;
  for (rit = kPrefixes.rbegin(); rit < kPrefixes.rend(); ++rit) {
    if (base >= float(rit->first)) break;
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
std::string to_string_with_precision(const T& value, const int n = 6) {
  std::ostringstream ss;
  ss.precision(n);
  ss << std::fixed << value;
  return std::move(ss).str();
}

/**
 * @brief Convert ftxui::Event to an user-friendly string
 * @param e UI event (mouse/keyboard input)
 * @return Formatted string
 */
inline std::string EventToString(const ftxui::Event& e) {
  if (e.is_character()) return e.character();

  if (e == ftxui::Event::ArrowUp) return "ArrowUp";
  if (e == ftxui::Event::ArrowDown) return "ArrowDown";
  if (e == ftxui::Event::ArrowRight) return "ArrowRight";
  if (e == ftxui::Event::ArrowLeft) return "ArrowLeft";
  if (e == ftxui::Event::PageUp) return "PageUp";
  if (e == ftxui::Event::PageDown) return "PageDown";
  if (e == ftxui::Event::Home) return "Home";
  if (e == ftxui::Event::End) return "End";
  if (e == ftxui::Event::Tab) return "Tab";
  if (e == ftxui::Event::TabReverse) return "Shift+Tab";
  if (e == ftxui::Event::Return) return "Return";
  if (e == ftxui::Event::Escape) return "Escape";

  return "Unknown";
}

}  // namespace util
#endif  // INCLUDE_UTIL_PREFIX_FORMATTER_H_