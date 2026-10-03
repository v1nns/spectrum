#include "view/base/dialog.h"

#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/terminal.hpp"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

Dialog::Dialog(const std::shared_ptr<EventDispatcher>& dispatcher, const Size& size,
               const Style& style)
    : dispatcher_{dispatcher}, size_{size}, style_{style} {
  if (size.min_line) size_.min_line += kBorderSize;
  if (size.min_column) size_.min_column += kBorderSize;
}

/* ********************************************************************************************** */

ftxui::Dimensions Dialog::CalculateSize(const ftxui::Dimensions& curr_size) const {
  // Calculate both width and height
  int width = curr_size.dimx * size_.width;
  int height = curr_size.dimy * size_.height;

  // Check if it is not below the minimum value
  if (size_.min_column && width < size_.min_column) width = size_.min_column;
  if (size_.min_line && height < size_.min_line) height = size_.min_line;

  // Check if it is not above the maximum value
  if (size_.max_column && width > size_.max_column) width = size_.max_column;
  if (size_.max_line && height > size_.max_line) height = size_.max_line;

  return ftxui::Dimensions{.dimx = width, .dimy = height};
}

/* ********************************************************************************************** */

ftxui::Element Dialog::Render(const ftxui::Dimensions& curr_size) const {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::WIDTH;

  const auto [width, height] = CalculateSize(curr_size);

  // Create border decorator style
  const auto& theme = GetTheme().dialog;
  auto border_decorator = ftxui::borderStyled(ftxui::DOUBLE, theme.border);

  // Create dialog decorator style
  auto decorator = ftxui::size(HEIGHT, EQUAL, height) | ftxui::size(WIDTH, EQUAL, width) |
                   ftxui::bgcolor(theme.*style_.background) |
                   ftxui::color(theme.*style_.foreground);

  // Keep an empty margin around dialog border, otherwise it would be merged with the borders
  // from blocks behind it (as both are drawn using box characters)
  return RenderImpl(curr_size) | border_decorator | decorator | ftxui::borderEmpty |
         ftxui::clear_under | ftxui::center;
}

/* ********************************************************************************************** */

bool Dialog::OnEvent(const ftxui::Event& event) {
  if (event.is_mouse() && OnMouseEventImpl(event)) {
    return true;
  }

  if (OnEventImpl(event)) {
    return true;
  }

  if (event == keybinding::Navigation::Escape || event == keybinding::Navigation::Close) {
    Close();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

std::shared_ptr<EventDispatcher> Dialog::GetDispatcher() const { return dispatcher_.lock(); }

}  // namespace interface
