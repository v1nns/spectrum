#include <cstdlib>  // for EXIT_SUCCESS
#include <memory>   // for make_unique, unique_ptr

#include "ui/base/terminal.h"  // for Terminal

using Terminal = std::shared_ptr<interface::Terminal>;  //!< Smart pointer to terminal

int main() {
  // Create a new terminal window
  Terminal term = std::make_shared<interface::Terminal>();

  // Initialize UI for terminal
  term->Init();

  // Start graphical interface loop
  term->Loop();

  return EXIT_SUCCESS;
}