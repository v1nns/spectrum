/**
 * \file
 * \brief  Header with utilities to be used within unit tests
 */

#ifndef INCLUDE_TEST_GENERAL_UTILS_H_
#define INCLUDE_TEST_GENERAL_UTILS_H_

#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/screen/terminal.hpp"
#include "util/formatter.h"
#include "view/element/style.h"

namespace utils {

//! Filter any ANSI escape code from string
inline std::string FilterAnsiCommands(const std::string& screen) {
  std::stringstream result;
  const std::regex ansi_command("(\e\\[(\\d+;)*(\\d+)?[ABCDHJKfmsu])|(\\r)");

  std::regex_replace(std::ostream_iterator<char>(result), screen.begin(), screen.end(),
                     ansi_command, "");

  // For aesthetics, add a newline in the beginning
  return result.str().insert(0, 1, '\n');
}

/* ********************************************************************************************** */

//! Split string into characters and send each as an event to Component
template <typename T>
inline void QueueCharacterEvents(T& component, const std::string& typed) {
  std::for_each(typed.begin(), typed.end(),
                [&component](char const& c) { component.OnEvent(ftxui::Event::Character(c)); });
}

/* ********************************************************************************************** */

//! Create an empty file on the given path (or truncate it, if already exists)
inline void CreateEmptyFile(const std::filesystem::path& path) {
  std::ofstream file{path};
  file.close();
}

/* ********************************************************************************************** */

//! Split string by line, trim its content and join it again
// NOTE: This was specially implemented for dialog rendering, in which may contain multiple empty
// spaces because of size delimitation
inline std::string FilterEmptySpaces(const std::string& raw) {
  std::istringstream input{raw};
  std::ostringstream output;

  // For aesthetics, add a newline in the beginning
  output << "\n";

  for (std::string line; std::getline(input, line);) {
    if (std::string trimmed = util::trim(line); !trimmed.empty()) {
      output << trimmed << "\n";
    }
  }

  return std::move(output).str();
}

/* ********************************************************************************************** */

/**
 * @brief Enable true colors while a test changes theme, and restore default theme when it finishes
 * (theme is shared by all tests). Create it before any color used by test.
 */
class ThemeGuard {
 public:
  ThemeGuard() : color_support_{ftxui::Terminal::ColorSupport()} {
    ftxui::Terminal::SetColorSupport(ftxui::Terminal::Color::TrueColor);
  }

  ~ThemeGuard() {
    ftxui::Terminal::SetColorSupport(color_support_);
    interface::SetTheme(interface::GetThemes().front().colors);
  }

  ThemeGuard(const ThemeGuard&) = delete;
  ThemeGuard& operator=(const ThemeGuard&) = delete;

 private:
  ftxui::Terminal::Color color_support_;  //!< Color support from before the test
};

/* ********************************************************************************************** */

//! Create a color that is not used by default theme (a different one for each identifier)
inline ftxui::Color MarkerColor(uint8_t id) { return ftxui::Color::RGB(1, 2, id); }

/* ********************************************************************************************** */

//! Create colors for a button using the given color in every state
inline interface::Theme::ButtonStates AllButtonStates(const ftxui::Color& color) {
  const interface::Theme::State state{.foreground = color, .background = color, .border = color};

  return interface::Theme::ButtonStates{
      .normal = state,
      .focused = state,
      .selected = state,
      .pressed = state,
      .disabled = state,
      .highlight = state,
  };
}

/* ********************************************************************************************** */

//! Check if any cell from screen uses the given color (as foreground or background)
inline bool HasColor(ftxui::Screen& screen, const ftxui::Color& color) {
  for (int y = 0; y < screen.dimy(); ++y) {
    for (int x = 0; x < screen.dimx(); ++x) {
      const auto& pixel = screen.PixelAt(x, y);
      if (pixel.foreground_color == color || pixel.background_color == color) return true;
    }
  }

  return false;
}

}  // namespace utils
#endif  // INCLUDE_TEST_GENERAL_UTILS_H_
