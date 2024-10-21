#include "view/base/dialog.h"

#include "ftxui/dom/elements.hpp"
#include "view/base/keybinding.h"

namespace interface {

Dialog::Dialog(const Size& size, const Style& style) : size_{size}, style_{style} {
  if (size.min_line) size_.min_line += kBorderSize;
  if (size.min_column) size_.min_column += kBorderSize;
}

/* ********************************************************************************************** */

ftxui::Element Dialog::Render(const ftxui::Dimensions& curr_size) const {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::WIDTH;

  // Calculate both width and height
  int width = curr_size.dimx * size_.width;
  int height = curr_size.dimy * size_.height;

  // Check if it is not below the minimum value
  if (size_.min_column && width < size_.min_column) width = size_.min_column;
  if (size_.min_line && height < size_.min_line) height = size_.min_line;

  // Check if it is not above the maximum value
  if (size_.max_column && width > size_.max_column) width = size_.max_column;
  if (size_.max_line && height > size_.max_line) height = size_.max_line;

  // Create border decorator style
  auto border_decorator = ftxui::borderStyled(ftxui::DOUBLE, ftxui::Color::Grey85);

  // Create dialog decorator style
  auto decorator = ftxui::size(HEIGHT, EQUAL, height) | ftxui::size(WIDTH, EQUAL, width) |
                   ftxui::bgcolor(style_.background) | ftxui::color(style_.foreground) |
                   ftxui::clear_under | ftxui::center;

  return RenderImpl(curr_size) | border_decorator | decorator;
}

/* ********************************************************************************************** */

bool Dialog::OnEvent(const ftxui::Event& event) {
  if (event.is_mouse()) {
    return OnMouseEventImpl(event);
  }

  using Keybind = keybinding::Navigation;
  if (event == Keybind::Escape || event == Keybind::Close) {
    Close();
  }

  // Let derived handle it
  return OnEventImpl(event);
}

}  // namespace interface
