/**
 * \file
 * \brief Header for UI style
 */

#ifndef INCLUDE_VIEW_ELEMENT_STYLE_H_
#define INCLUDE_VIEW_ELEMENT_STYLE_H_

#include <ftxui/dom/elements.hpp>

namespace interface {

//! Custom style for menu entry
struct MenuEntryOption {
  ftxui::Decorator normal;
  ftxui::Decorator focused;
  ftxui::Decorator selected;
  ftxui::Decorator selected_focused;
};

//! Decorator for custom style to apply on menu entry
inline MenuEntryOption Colored(ftxui::Color c) {
  using ftxui::color;
  using ftxui::Decorator;
  using ftxui::inverted;

  return MenuEntryOption{
      .normal = Decorator(color(c)),
      .focused = Decorator(color(c)) | inverted,
      .selected = Decorator(color(c)) | inverted,
      .selected_focused = Decorator(color(c)) | inverted,
  };
}

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_STYLE_H_
