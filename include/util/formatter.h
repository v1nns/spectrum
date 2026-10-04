/**
 * \file
 * \brief  Class for formatting values to pretty-printable strings
 */

#ifndef INCLUDE_UTIL_PREFIX_FORMATTER_H_
#define INCLUDE_UTIL_PREFIX_FORMATTER_H_

#include <math.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>

#include "ftxui/component/event.hpp"

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
  if (e == ftxui::Event::Delete) return "Delete";
  if (e == ftxui::Event::F1) return "F1";
  if (e == ftxui::Event::F2) return "F2";
  if (e == ftxui::Event::F3) return "F3";
  if (e == ftxui::Event::F4) return "F4";
  if (e == ftxui::Event::F5) return "F5";
  if (e == ftxui::Event::F6) return "F6";
  if (e == ftxui::Event::F7) return "F7";
  if (e == ftxui::Event::F8) return "F8";
  if (e == ftxui::Event::F9) return "F9";
  if (e == ftxui::Event::F10) return "F10";
  if (e == ftxui::Event::F11) return "F11";
  if (e == ftxui::Event::F12) return "F12";

  if (e == ftxui::Event::Character(' ')) return "Space";
  if (e.is_character()) return e.character();

  return "Unknown";
}

/**
 * @brief Remove whitespace from left end of string
 * @param s Raw string
 * @return Formatted string
 */
inline std::string ltrim(const std::string& s) {
  size_t start = s.find_first_not_of(" \n\r\t\f\v");
  return (start == std::string::npos) ? "" : s.substr(start);
}

/**
 * @brief Remove whitespace from right end of string
 * @param s Raw string
 * @return Formatted string
 */
inline std::string rtrim(const std::string& s) {
  size_t end = s.find_last_not_of(" \n\r\t\f\v");
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

/**
 * @brief Remove whitespace from both ends of string
 * @param s Raw string
 * @return Formatted string
 */
inline std::string trim(const std::string& s) { return rtrim(ltrim(s)); }

/**
 * @brief Compare a single character in undercase
 * @param a Character a
 * @param b Character b
 * @return true if 'a' is equal to 'b', false otherwise
 */
inline bool compare(const char& a, const char& b) { return std::tolower(a) == std::tolower(b); };

/**
 * @brief Search for a substring in the given string
 * @param string Raw string
 * @param substring Substring to search for
 * @return true if 'substring' exists in 'string', false otherwise
 */
inline bool contains(std::string_view string, std::string_view substring) {
  auto it = std::search(string.begin(), string.end(), substring.begin(), substring.end(), compare);
  return it != string.end();
}

/**
 * @brief Check if given character is an emoji
 * @param wc Unicode code point
 * @return True if character is an emoji, false otherwise
 */
inline bool is_emoji(const char32_t& wc) {
  // Common emoji Unicode ranges
  return (wc >= 0x1F600 && wc <= 0x1F64F) ||  // Emoticons
         (wc >= 0x1F300 && wc <= 0x1F5FF) ||  // Symbols & Pictographs
         (wc >= 0x1F680 && wc <= 0x1F6FF) ||  // Transport & Map Symbols
         (wc >= 0x1F1E0 && wc <= 0x1F1FF) ||  // Flags
         (wc >= 0x1F900 && wc <= 0x1FAFF) ||  // Supplemental Symbols & Pictographs (and extended)
         (wc >= 0x2600 && wc <= 0x27BF) ||    // Miscellaneous Symbols & Dingbats
         wc == 0xFE0F || wc == 0x200D;        // Emoji variation selector & zero width joiner
}

/**
 * @brief Filter UTF-8 string to remove emojis (and any invalid byte sequence), keeping all the
 * other characters (e.g. accented letters, cyrillic, kanji)
 * @param s Raw string (encoded as UTF-8)
 * @return Formatted string
 */
inline std::string filter_emoji(const std::string& s) {
  static constexpr unsigned char kAsciiLimit = 0x80;        //!< First byte value out of ASCII
  static constexpr unsigned char kContinuationMask = 0xC0;  //!< Bits identifying continuation byte
  static constexpr unsigned char kContinuationTag = 0x80;   //!< Expected value for those bits
  static constexpr unsigned char kContinuationData = 0x3F;  //!< Bits with data in continuation byte
  static constexpr int kContinuationBits = 6;               //!< Data bits in continuation byte

  //! Sequence length based on leading byte, as {mask, expected value, data bits, length}
  struct Leading {
    unsigned char mask, tag, data;
    size_t length;
  };

  static constexpr std::array<Leading, 3> kLeading{{
      {0xE0, 0xC0, 0x1F, 2},
      {0xF0, 0xE0, 0x0F, 3},
      {0xF8, 0xF0, 0x07, 4},
  }};

  std::string filtered;
  filtered.reserve(s.size());

  for (size_t i = 0; i < s.size();) {
    const auto first = static_cast<unsigned char>(s[i]);

    // ASCII character, there is nothing to decode
    if (first < kAsciiLimit) {
      filtered.push_back(s[i++]);
      continue;
    }

    auto leading = std::find_if(kLeading.begin(), kLeading.end(),
                                [first](const Leading& l) { return (first & l.mask) == l.tag; });

    // Decode code point from multibyte sequence
    bool valid = leading != kLeading.end() && i + leading->length <= s.size();
    char32_t code_point = valid ? static_cast<char32_t>(first & leading->data) : 0;

    for (size_t j = 1; valid && j < leading->length; j++) {
      const auto next = static_cast<unsigned char>(s[i + j]);
      valid = (next & kContinuationMask) == kContinuationTag;
      code_point =
          (code_point << kContinuationBits) | static_cast<char32_t>(next & kContinuationData);
    }

    // Skip invalid byte and try to decode again from the next one
    if (!valid) {
      i++;
      continue;
    }

    if (!is_emoji(code_point)) filtered.append(s, i, leading->length);
    i += leading->length;
  }

  return filtered;
}

}  // namespace util
#endif  // INCLUDE_UTIL_PREFIX_FORMATTER_H_
