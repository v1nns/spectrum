/**
 * \file
 * \brief Header for UI utils
 */

#ifndef INCLUDE_VIEW_ELEMENT_UTIL_H_
#define INCLUDE_VIEW_ELEMENT_UTIL_H_

#include <algorithm>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
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
 * @brief Node that renders elements side by side, with the same space between them (and also
 * before the first one and after the last one). Columns that cannot be shared equally are split
 * between both ends, so distance from one element to the next is always the same
 */
class SpacedRow : public ftxui::Node {
 public:
  explicit SpacedRow(ftxui::Elements children) : ftxui::Node(std::move(children)) {}

  void ComputeRequirement() override {
    requirement_ = ftxui::Requirement{};

    for (const auto& child : children_) {
      child->ComputeRequirement();

      requirement_.min_x += child->requirement().min_x;
      requirement_.min_y = std::max(requirement_.min_y, child->requirement().min_y);
    }
  }

  void SetBox(ftxui::Box box) override {
    ftxui::Node::SetBox(box);

    const int width = box.x_max - box.x_min + 1;
    const int spaces = static_cast<int>(children_.size()) + 1;
    const int available = std::max(0, width - requirement_.min_x);
    const int gap = available / spaces;

    int x = box.x_min + gap + ((available % spaces) / 2);

    for (const auto& child : children_) {
      ftxui::Box child_box = box;
      child_box.x_min = x;
      child_box.x_max = x + child->requirement().min_x - 1;
      child->SetBox(child_box);

      x = child_box.x_max + 1 + gap;
    }
  }
};

//! Render elements side by side, with the same space between them
inline ftxui::Element spaced_row(ftxui::Elements elements) {
  return std::make_shared<SpacedRow>(std::move(elements));
}

/**
 * @brief Node that sets foreground or background color on every cell from its child. Unlike
 * ftxui::color and ftxui::bgcolor, default color (from terminal) also replaces the one below it,
 * instead of being transparent (as it is since FTXUI 6)
 */
class OpaqueColor : public ftxui::Node {
 public:
  OpaqueColor(ftxui::Element child, ftxui::Color color, bool background)
      : ftxui::Node({std::move(child)}), color_{color}, background_{background} {}

  void SetBox(ftxui::Box box) override {
    ftxui::Node::SetBox(box);
    children_[0]->SetBox(box);
  }

  void Render(ftxui::Screen& screen) override {
    for (int y = box_.y_min; y <= box_.y_max; ++y) {
      for (int x = box_.x_min; x <= box_.x_max; ++x) {
        auto& cell = screen.CellAt(x, y);
        (background_ ? cell.background_color : cell.foreground_color) = color_;
      }
    }

    ftxui::Node::Render(screen);
  }

 private:
  ftxui::Color color_;
  bool background_;
};

//! Same as ftxui::color, but default color (from terminal) is not transparent
inline ftxui::Decorator opaque_color(ftxui::Color color) {
  return [color](ftxui::Element child) {
    return std::make_shared<OpaqueColor>(std::move(child), color, /*background=*/false);
  };
}

//! Same as ftxui::bgcolor, but default color (from terminal) is not transparent
inline ftxui::Decorator opaque_bgcolor(ftxui::Color color) {
  return [color](ftxui::Element child) {
    return std::make_shared<OpaqueColor>(std::move(child), color, /*background=*/true);
  };
}

/**
 * @brief Node that renders a window (content with border) and its title over the top border line,
 * taking the whole width of it (between both corners)
 */
class FullTitleWindow : public ftxui::Node {
 public:
  FullTitleWindow(ftxui::Element window, ftxui::Element title)
      : ftxui::Node({std::move(window), std::move(title)}) {}

  void ComputeRequirement() override {
    // Same requirement from window (including flex), but wide enough to fit title too
    ftxui::Node::ComputeRequirement();
    requirement_.min_x = std::max(requirement_.min_x, children_[1]->requirement().min_x + 2);
  }

  void SetBox(ftxui::Box box) override {
    ftxui::Node::SetBox(box);
    children_[0]->SetBox(box);

    ftxui::Box title_box = box;
    title_box.x_min++;
    title_box.x_max--;
    title_box.y_max = title_box.y_min;
    children_[1]->SetBox(title_box);
  }
};

/**
 * @brief Same as ftxui::window, but title takes the whole width of the top border line, so it can
 * be split by a filler (e.g. to align buttons to the right). Since FTXUI 6, ftxui::window keeps
 * title with only the width it requires
 * @param title Element drawn over the top border line (border is kept where it draws nothing)
 * @param content Element drawn inside border
 * @return Content with border and title
 */
inline ftxui::Element full_title_window(ftxui::Element title, ftxui::Element content) {
  return std::make_shared<FullTitleWindow>(std::move(content) | ftxui::borderRounded,
                                           std::move(title));
}

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_UTIL_H_
