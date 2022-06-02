#include <cstdlib>  // for EXIT_SUCCESS
#include <memory>   // for make_unique, unique_ptr

#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "model/global_resource.h"
#include "view/base/terminal.h"  // for Terminal

int main() {
  auto shared_data = std::make_shared<model::GlobalResource>();

  // Create and initialize a new player
  auto player = audio::Player::Create(shared_data);

  // Create and initialize a new terminal window
  auto terminal = interface::Terminal::Create(shared_data);

  // Create a full-size screen and register exit callback
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
  terminal->RegisterExitCallback(screen.ExitLoopClosure());

  // Start graphical interface loop
  screen.Loop(terminal);

  return EXIT_SUCCESS;
}