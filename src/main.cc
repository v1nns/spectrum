#include <cstdlib>  // for EXIT_SUCCESS
#include <memory>   // for make_unique, unique_ptr

#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "view/base/terminal.h"                    // for Terminal

int main() {
  // Create and initialize a new terminal window
  auto terminal = interface::Terminal::Create();

  // Create a full-size screen and register exit callback
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  terminal->RegisterExitCallback(screen.ExitLoopClosure());

  // Start graphical interface loop
  screen.Loop(terminal);

  return EXIT_SUCCESS;
}