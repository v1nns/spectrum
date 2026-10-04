/**
 * \file
 * \brief Header for UI utils
 */

#ifndef INCLUDE_VIEW_ELEMENT_UTIL_H_
#define INCLUDE_VIEW_ELEMENT_UTIL_H_

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace interface {

//! Similar to std::clamp, but allow hi to be lower than lo.
template <class T>
inline constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

//! Returns a decorator that sets the size of an element to the specified width and height.
inline const ftxui::Decorator set_size(int width, int height) {
  return ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width) |
         ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, height);
};

/**
 * @brief Truncate text to fit in the given number of columns, ending it with an ellipsis when cut.
 * Width is measured in terminal columns, so it works with multi-byte and full-width characters
 * @param text Content to fit
 * @param max_columns Maximum number of columns available
 * @return Text that fits in the given columns
 */
inline std::string ellipsize(const std::string& text, int max_columns) {
  static constexpr std::string_view kEllipsis = "…";
  static constexpr int kEllipsisWidth = 1;

  if (max_columns <= 0) {
    return "";
  }

  if (ftxui::string_width(text) <= max_columns) {
    return text;
  }

  std::string result;
  int used = 0;

  // Keep as many glyphs as possible, leaving room for the ellipsis
  for (const auto& glyph : ftxui::Utf8ToGlyphs(text)) {
    const int width = glyph.empty() ? 0 : ftxui::string_width(glyph);
    if (used + width > max_columns - kEllipsisWidth) {
      break;
    }

    result += glyph;
    used += width;
  }

  return result + std::string(kEllipsis);
}

/**
 * @brief Shorten path to fit in the given number of columns, keeping its end (e.g.
 * ".../artists/aphex"). When possible, it starts at a directory separator, so no directory name is
 * cut; otherwise (last directory name is too long by itself), it keeps the end of that name.
 * Width is measured in terminal columns, so it works with multi-byte and full-width characters
 * @param path Path to fit
 * @param max_columns Maximum number of columns available
 * @return Path that fits in the given columns
 */
inline std::string shorten_path(const std::string& path, int max_columns) {
  static constexpr std::string_view kEllipsis = "...";
  static constexpr int kEllipsisWidth = 3;

  if (ftxui::string_width(path) <= max_columns) {
    return path;
  }

  const int available = max_columns - kEllipsisWidth;
  if (available <= 0) {
    return std::string(kEllipsis.substr(0, std::max(max_columns, 0)));
  }

  // Keep as many glyphs as possible from the end of path
  const auto glyphs = ftxui::Utf8ToGlyphs(path);
  auto begin = glyphs.end();

  for (int used = 0; begin != glyphs.begin();) {
    const int width = ftxui::string_width(*std::prev(begin));
    if (used + width > available) break;

    used += width;
    --begin;
  }

  std::string tail;
  for (auto it = begin; it != glyphs.end(); ++it) tail += *it;

  // Prefer to start from a directory separator, so the first directory name is not cut
  if (auto separator = tail.find('/'); separator != std::string::npos) {
    tail = tail.substr(separator);
  }

  return std::string(kEllipsis) + tail;
}

/**
 * @brief Node that renders the preferred element only if it fits in the width given by parent,
 * otherwise renders the fallback element. It requests only the fallback size from its parent, so
 * it never forces other elements (e.g. neighbour blocks) to shrink
 */
class FitOrFallback : public ftxui::Node {
  static constexpr int kPreferred = 0;  //!< Index for preferred element
  static constexpr int kFallback = 1;   //!< Index for fallback element

 public:
  FitOrFallback(ftxui::Element preferred, ftxui::Element fallback)
      : ftxui::Node({std::move(preferred), std::move(fallback)}) {}

  void ComputeRequirement() override {
    ftxui::Node::ComputeRequirement();
    requirement_ = children_.at(kFallback)->requirement();
  }

  void SetBox(ftxui::Box box) override {
    ftxui::Node::SetBox(box);

    const int width = box.x_max - box.x_min + 1;
    active_ = width >= children_.at(kPreferred)->requirement().min_x ? kPreferred : kFallback;

    children_.at(active_)->SetBox(box);
  }

  void Render(ftxui::Screen& screen) override { children_.at(active_)->Render(screen); }

 private:
  int active_ = kPreferred;  //!< Element chosen to be rendered
};

//! Render preferred element if it fits in the available width, otherwise render fallback element
inline ftxui::Element fit_or_fallback(ftxui::Element preferred, ftxui::Element fallback) {
  return std::make_shared<FitOrFallback>(std::move(preferred), std::move(fallback));
}

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_UTIL_H_
