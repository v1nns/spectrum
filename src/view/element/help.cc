#include "view/element/help.h"

namespace interface {

Help::Help()
    : style_{DialogStyle{
          .background = ftxui::Color::Yellow4Bis,
          .foreground = ftxui::Color::Grey93,
      }},
      opened_{false},
      message_{} {}

/* ********************************************************************************************** */

ftxui::Element Help::Render(const ftxui::Dimensions max_size) {
  using ftxui::WIDTH, ftxui::HEIGHT, ftxui::EQUAL;
  int width = max_size.dimx * 0.6;
  int height = max_size.dimy * 0.4;

  return ftxui::hbox({
             ftxui::vbox({
                 ftxui::text("files") | ftxui::bold,
                 ftxui::text("/ - Search for file"),
                 ftxui::text("h/j/k/l - Navigate on list"),
             }),

             ftxui::vbox({
                 ftxui::text(" HELP ") | ftxui::bold,
             }),

             ftxui::vbox({
                 ftxui::text("wtf"),
             }),

         }) |
         ftxui::bgcolor(style_.background) | ftxui::size(HEIGHT, EQUAL, height) |
         ftxui::size(WIDTH, EQUAL, width) | ftxui::borderDouble | ftxui::color(style_.foreground) |
         ftxui::center;
}

/* ********************************************************************************************** */

bool Help::OnEvent(ftxui::Event event) {
  if (event == ftxui::Event::Return || event == ftxui::Event::Escape ||
      event == ftxui::Event::Character('q')) {
    Clear();
  }

  // This is to ensure that no one else will treat any event while on error mode
  return true;
}

/* ********************************************************************************************** */

void Help::Show() { opened_ = true; }

/* ********************************************************************************************** */

void Help::Clear() {
  opened_ = false;
  message_.clear();
}

}  // namespace interface
