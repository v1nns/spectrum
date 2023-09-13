#include "view/element/help.h"

#include "view/base/keybinding.h"

namespace interface {

ftxui::Element Help::Render() const {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::WIDTH;

  auto decorator = ftxui::size(HEIGHT, EQUAL, kMaxLines) | ftxui::size(WIDTH, EQUAL, kMaxColumns) |
                   ftxui::borderDouble | ftxui::bgcolor(style_.background) |
                   ftxui::color(style_.foreground) | ftxui::clear_under | ftxui::center;

  auto content = active_ == View::General ? BuildGeneralInfo() : BuildTabInfo();

  return content | decorator;
}

/* ********************************************************************************************** */

bool Help::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;
  if (event == Keybind::Return || event == Keybind::Escape || event == Keybind::Close) {
    Close();
  }

  // This is to ensure that no one else will treat any event while helper is opened
  return true;
}

/* ********************************************************************************************** */

void Help::ShowGeneralInfo() {
  active_ = View::General;
  opened_ = true;
}

/* ********************************************************************************************** */

void Help::ShowTabInfo() {
  active_ = View::Tab;
  opened_ = true;
}

/* ********************************************************************************************** */

void Help::Close() { opened_ = false; }

/* ********************************************************************************************** */

ftxui::Element Help::title(const std::string& message) const {
  return ftxui::vbox({
      ftxui::text(""),
      ftxui::text(message) | ftxui::color(ftxui::Color::Black) | ftxui::bold | ftxui::xflex_grow,
      ftxui::text(""),
  });
}

/* ********************************************************************************************** */

ftxui::Element Help::command(const std::string& keybind, const std::string& description) const {
  return ftxui::hbox({
      ftxui::text(keybind) | ftxui::color(ftxui::Color::PaleTurquoise1),
      ftxui::text(!keybind.empty() ? " - " : ""),
      ftxui::text(description) | ftxui::color(ftxui::Color::Black),
  });
}

/* ********************************************************************************************** */

ftxui::Element Help::BuildGeneralInfo() const {
  constexpr auto lateral_margin = []() { return ftxui::vbox({ftxui::text("   ")}); };

  constexpr auto vertical_margin = []() { return ftxui::vbox({ftxui::text("")}); };

  auto block_decorator = ftxui::color(ftxui::Color::Black) | ftxui::xflex_grow;

  static auto block_title =
      ftxui::text("General") | ftxui::color(ftxui::Color::Black) | ftxui::bold | ftxui::center;

  static auto content = ftxui::gridbox({
      {
          lateral_margin(),

          // Column 1
          ftxui::vbox({

              vertical_margin(),

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

          lateral_margin(),

          // Column 2
          ftxui::vbox({

              vertical_margin(),

              title("tab view"),
              command("F2", "Show tab helper"),
              command("1", "Focus visualizer"),
              command("2", "Focus equalizer"),
              command("3", "Focus lyric"),

              title("player"),
              command("p", "Pause/Resume current song"),
              command("s", "Stop current song"),
              command("c", "Clear current song"),
              command("+/-", "Increase/decrease volume"),
              command("m", "Toggle volume mute"),
              command("f", "Seek forward position in current song"),
              command("b", "Seek backward position in current song"),
              command("</>", "Skip to previous/next song"),

          }) | block_decorator,

          lateral_margin(),
      },
  });

  return ftxui::vbox({vertical_margin(), block_title, content});
}

/* ********************************************************************************************** */

ftxui::Element Help::BuildTabInfo() const {
  constexpr auto margin = []() { return ftxui::text(""); };

  constexpr auto vertical_margin = []() {
    return ftxui::vbox({
        ftxui::text(""),
        ftxui::text(""),
    });
  };

  auto block_decorator = ftxui::color(ftxui::Color::Black) | ftxui::center;

  static auto block_title =
      ftxui::text("Tab pages") | ftxui::color(ftxui::Color::Black) | ftxui::bold | ftxui::center;

  static auto content = ftxui::vbox({

                            margin(),

                            title("visualizer"),
                            command("a", "Change spectrum animation"),
                            command("h", "Hide other blocks"),

                            vertical_margin(),

                            title("equalizer"),
                            command("←/↓/↑/→", "Navigate on elements"),
                            command("h/j/k/l", "Navigate on elements"),
                            command("Space/Return", "Open/close picker"),
                            command("            ", "Select new preset"),
                            command("Esc", "Cancel focus"),
                            command("a", "Apply equalizer settings"),
                            command("r", "Reset equalizer settings"),

                            vertical_margin(),

                            title("lyrics"),
                            command("", "N/A"),

                        }) |
                        block_decorator;

  return ftxui::vbox({margin(), block_title, content});
}

}  // namespace interface
