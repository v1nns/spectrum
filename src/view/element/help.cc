#include "view/element/help.h"

namespace interface {

Help::Help()
    : style_{DialogStyle{
          .background = ftxui::Color::BlueLight,
          .foreground = ftxui::Color::Grey93,
      }},
      opened_{false} {}

/* ********************************************************************************************** */

ftxui::Element Help::Render() {
  using ftxui::WIDTH, ftxui::HEIGHT, ftxui::EQUAL;

  constexpr auto title = [](const std::string& title) {
    return ftxui::vbox({
        ftxui::text(""),
        ftxui::text(title) | ftxui::color(ftxui::Color::Black) | ftxui::bold | ftxui::xflex_grow,
        ftxui::text(""),
    });
  };

  constexpr auto command = [](const std::string& command, const std::string& description) {
    return ftxui::hbox({
        ftxui::text(command) | ftxui::color(ftxui::Color::PaleTurquoise1),
        ftxui::text(!command.empty() ? " - " : ""),
        ftxui::text(description) | ftxui::color(ftxui::Color::Black),
    });
  };

  constexpr auto margin = []() { return ftxui::vbox({ftxui::text("   ")}); };

  auto block_decorator = ftxui::color(ftxui::Color::Black) | ftxui::xflex_grow;

  auto decorator = ftxui::size(HEIGHT, EQUAL, kMaxLines) | ftxui::size(WIDTH, EQUAL, kMaxColumns) |
                   ftxui::borderDouble | ftxui::bgcolor(style_.background) |
                   ftxui::color(style_.foreground) | ftxui::clear_under | ftxui::center;

  static ftxui::Element content =
      ftxui::gridbox({
          {
              margin(),

              // Column 1
              ftxui::vbox({
                  title("block focus"),
                  command("Shift+1", "files"),
                  command("Shift+2", "information"),
                  command("Shift+3", "visualizer/equalizer"),
                  command("Shift+4", "player"),
                  command("Tab", "Focus next block"),
                  command("Shift+Tab", "Focus previous block"),

                  title("files"),
                  command("←/↓/↑/→", "Navigate on list"),
                  command("h/j/k/l", "Navigate on list"),
                  command("Home", "Go to first entry"),
                  command("End", "Go to last entry"),
                  command("/", "Enter search mode"),
                  command("Esc", "Cancel search mode"),
                  command("Return", "Enter directory/play song"),

                  title("information"),
                  command("", "N/A"),

              }) | block_decorator,

              margin(),

              // Column 2
              ftxui::vbox({
                  title("visualizer"),
                  command("a", "Change spectrum animation"),

                  margin(),

                  title("equalizer"),
                  command("←/↓/↑/→", "Navigate on frequency bars"),
                  command("h/j/k/l", "Navigate on frequency bars"),
                  command("Esc", "Cancel focus"),
                  command("a", "Apply equalizer settings"),
                  command("r", "Reset equalizer settings"),

                  margin(),

                  title("player"),
                  command("p", "Pause/Resume current song"),
                  command("s", "Stop current song"),
                  command("c", "Clear current song"),
                  command("+/-", "Increase/decrease volume"),
                  command("m", "Toggle volume mute"),
                  command("f", "Seek forward position in current song"),
                  command("b", "Seek backward position in current song"),
              }) | block_decorator,

              margin(),
          },
      }) |
      decorator;

  return content;
}

/* ********************************************************************************************** */

bool Help::OnEvent(ftxui::Event event) {
  if (event == ftxui::Event::Return || event == ftxui::Event::Escape ||
      event == ftxui::Event::Character('q')) {
    Close();
  }

  // This is to ensure that no one else will treat any event while helper is opened
  return true;
}

/* ********************************************************************************************** */

void Help::Show() { opened_ = true; }

/* ********************************************************************************************** */

void Help::Close() { opened_ = false; }

}  // namespace interface
