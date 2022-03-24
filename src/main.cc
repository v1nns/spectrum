#include <cstdlib>  // for EXIT_SUCCESS
#include <memory>   // for make_unique, unique_ptr

#include "ui/base/terminal.h"  // for Terminal

using Terminal =
    std::unique_ptr<interface::Terminal>;  //!< Smart pointer to have only one terminal instance

/* ********************************************************************************************** */

int main() {
  // Create a new terminal window
  Terminal term = std::make_unique<interface::Terminal>();

  // Initialize terminal screen
  term->Init();

  // Start graphical interface loop
  term->Loop();

  return EXIT_SUCCESS;
}