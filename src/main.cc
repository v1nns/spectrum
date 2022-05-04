#include <cstdlib>  // for EXIT_SUCCESS
#include <memory>   // for make_unique, unique_ptr

#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "view/base/terminal.h"                    // for Terminal

using Terminal = std::shared_ptr<interface::Terminal>;  //!< Smart pointer to terminal

int main() {
  // Create a new terminal window
  Terminal term = std::make_shared<interface::Terminal>();

  // Initialize terminal view
  term->Init();

  // Create a full-size screen and register exit callback
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  term->RegisterExitCallback(screen.ExitLoopClosure());

  // start graphical interface loop
  screen.Loop(term);

  return EXIT_SUCCESS;
}