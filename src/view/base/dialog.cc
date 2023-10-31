#include "view/base/dialog.h"

#include "view/base/keybinding.h"

namespace interface {

Dialog::Dialog(int max_columns, int max_lines, DialogStyle style)
    : max_columns_{max_columns}, max_lines_{max_lines}, style_{style} {}

/* ********************************************************************************************** */

ftxui::Element Dialog::Render() const {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::WIDTH;

  auto decorator = ftxui::size(HEIGHT, EQUAL, max_lines_) |
                   ftxui::size(WIDTH, EQUAL, max_columns_) | ftxui::borderDouble |
                   ftxui::bgcolor(style_.background) | ftxui::color(style_.foreground) |
                   ftxui::clear_under | ftxui::center;

  return RenderImpl() | decorator;
}

/* ********************************************************************************************** */

bool Dialog::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;
  if (event == Keybind::Return || event == Keybind::Escape || event == Keybind::Close) {
    Close();
  }

  // Let derived handle it
  return OnEventImpl(event);
}

}  // namespace interface
