/**
 * \file
 * \brief  Header with utilities to be used within unit tests
 */

#ifndef INCLUDE_TEST_GENERAL_UTILS_H_
#define INCLUDE_TEST_GENERAL_UTILS_H_

#include <iterator>
#include <regex>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "util/formatter.h"

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

}  // namespace utils
#endif  // INCLUDE_TEST_GENERAL_UTILS_H_
