/**
 * \file
 * \brief  Header with utilities to be used within unit tests
 */

#ifndef INCLUDE_TEST_UTILS_H_
#define INCLUDE_TEST_UTILS_H_

#include <iterator>
#include <regex>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"

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
inline void QueueCharacterEvents(ftxui::ComponentBase& block, const std::string& typed) {
  std::for_each(typed.begin(), typed.end(),
                [&block](char const& c) { block.OnEvent(ftxui::Event::Character(c)); });
}

}  // namespace utils
#endif  // INCLUDE_TEST_UTILS_H_
