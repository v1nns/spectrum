#include "view/element/help.h"

namespace interface {

Help::Help()
    : style_{DialogStyle{
          .background = ftxui::Color::BlueLight,
          .foreground = ftxui::Color::Grey93,
      }},
      opened_{false},
      message_{} {}

/* ********************************************************************************************** */

ftxui::Element Help::Render() {
  using ftxui::WIDTH, ftxui::HEIGHT, ftxui::EQUAL;
  auto title = [](const std::string& title) { return ftxui::text(title) | ftxui::bold; };

  auto command = [](const std::string& command, const std::string& description) {
    return ftxui::hbox({
        ftxui::text(command) | ftxui::color(ftxui::Color::PaleTurquoise1),
        ftxui::text(" - "),
        ftxui::text(description),
    });
  };

  return ftxui::hbox({
             ftxui::vbox({
                 ftxui::text(""),
                 title("files"),
                 ftxui::text(""),
                 command("←/↓/↑/→", "Navigate on list"),
                 command("h/j/k/l", "Navigate on list"),
                 command("Home", "Go to first entry"),
                 command("End", "Go to last entry"),
                 command("Tab", "Jump some entries"),
                 command("Shift+Tab", "Jump back some entries"),
                 command("/", "Enter search mode"),
                 command("Esc", "Cancel search mode"),
                 command("Return", "Enter directory OR play selected file"),
                 ftxui::text(""),
                 title("information"),
                 ftxui::text(""),
                 ftxui::text("N/A"),
             }) | ftxui::color(ftxui::Color::Black) |
                 ftxui::hcenter | ftxui::flex,

             ftxui::vbox({
                 ftxui::text(""),
                 title("visualizer"),
                 ftxui::text(""),
                 ftxui::text("N/A"),
                 ftxui::text(""),
                 title("player"),
                 ftxui::text(""),
                 command("p", "Pause/Resume current song"),
                 command("s", "Stop current song"),
                 command("c", "Clear current song"),
                 command("+/-", "Increase/decrease volume"),
                 command("f", "Seek forward position in current song"),
                 command("b", "Seek backward position in current song"),
                 ftxui::text(""),
             }) | ftxui::color(ftxui::Color::Black) |
                 ftxui::hcenter | ftxui::flex,

         }) |
         ftxui::bgcolor(style_.background) | ftxui::size(HEIGHT, EQUAL, kMaxLines) |
         ftxui::size(WIDTH, EQUAL, kMaxColumns) | ftxui::borderDouble |
         ftxui::color(style_.foreground) | ftxui::center;
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
