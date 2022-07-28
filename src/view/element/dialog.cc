#include "view/element/dialog.h"

namespace interface {

Dialog::Dialog()
    : style_{DialogStyle{
          .background = ftxui::Color::DarkRedBis,
          .foreground = ftxui::Color::Grey93,
      }},
      opened_{false},
      message_{} {}

/* ********************************************************************************************** */

ftxui::Element Dialog::Render() {
  using ftxui::WIDTH, ftxui::HEIGHT, ftxui::EQUAL;
  return ftxui::vbox({
             ftxui::text(" ERROR") | ftxui::bold,
             ftxui::text(""),
             ftxui::paragraph(message_) | ftxui::center | ftxui::bold,
         }) |
         ftxui::bgcolor(style_.background) | ftxui::size(HEIGHT, EQUAL, 5) |
         ftxui::size(WIDTH, EQUAL, 35) | ftxui::borderDouble | ftxui::color(style_.foreground) |
         ftxui::center;
}

/* ********************************************************************************************** */

bool Dialog::OnEvent(ftxui::Event event) {
  if (event == ftxui::Event::Return || event == ftxui::Event::Escape ||
      event == ftxui::Event::Character('q')) {
    Clear();
  }

  // This is to ensure that no one else will treat any event while on error mode
  return true;
}

/* ********************************************************************************************** */

void Dialog::SetErrorMessage(const std::string& message) {
  message_ = message;
  opened_ = true;
}

/* ********************************************************************************************** */

void Dialog::Clear() {
  opened_ = false;
  message_.clear();
}

}  // namespace interface
