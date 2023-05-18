#include "view/element/help.h"

namespace interface {

ftxui::Element Help::Render() const {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::WIDTH;

  constexpr auto title = [](const std::string& message) {
    return ftxui::vbox({
        ftxui::text(""),
        ftxui::text(message) | ftxui::color(ftxui::Color::Black) | ftxui::bold | ftxui::xflex_grow,
        ftxui::text(""),
    });
  };

  constexpr auto command = [](const std::string& keybind, const std::string& description) {
    return ftxui::hbox({
        ftxui::text(keybind) | ftxui::color(ftxui::Color::PaleTurquoise1),
        ftxui::text(!keybind.empty() ? " - " : ""),
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
                  command("Shift+1", "Focus files"),
                  command("Shift+2", "Focus information"),
                  command("Shift+3", "Focus tab viewer"),
                  command("Shift+4", "Focus player"),
                  command("Tab", "Focus next block"),
                  command("Shift+Tab", "Focus previous block"),
                  command("Esc", "Remove focus"),

                  title("files"),
                  command("←/↓/↑/→", "Navigate on list"),
                  command("h/j/k/l", "Navigate on list"),
                  command("Home", "Go to first entry"),
                  command("End", "Go to last entry"),
                  command("/", "Enter search mode"),
                  command("Esc", "Cancel search mode (when focused)"),
                  command("Return", "Enter directory/play song"),

                  title("information"),
                  command("", "N/A"),
              }) | block_decorator,

              margin(),

              // Column 2
              ftxui::vbox({
                  title("visualizer"),
                  command("a", "Change spectrum animation"),
                  command("h", "Hide other blocks"),

                  title("equalizer"),
                  command("←/↓/↑/→", "Navigate on elements"),
                  command("h/j/k/l", "Navigate on elements"),
                  command("Space/Return", "Open/close picker"),
                  command("            ", "Select new preset"),
                  command("Esc", "Cancel focus"),
                  command("a", "Apply equalizer settings"),
                  command("r", "Reset equalizer settings"),

                  title("lyrics"),
                  command("", "N/A"),

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

bool Help::OnEvent(const ftxui::Event& event) {
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
