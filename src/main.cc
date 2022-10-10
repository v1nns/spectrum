/**
 * \file
 * \brief Main function
 */
#include <cstdlib>  // for EXIT_SUCCESS

#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "middleware/media_controller.h"           // for MediaController
#include "view/base/terminal.h"                    // for Terminal

int main() {
  // Create and initialize a new player
  auto player = audio::Player::Create();

  // Create and initialize a new terminal window
  auto terminal = interface::Terminal::Create();

  // Create and initialize a new middleware for terminal and player
  auto middleware = middleware::MediaController::Create(terminal, player);

  // Create a full-size screen and register exit callback
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

  terminal->RegisterEventSenderCallback([&](ftxui::Event e) { screen.PostEvent(e); });

  terminal->RegisterExitCallback([&]() { screen.ExitLoopClosure()(); });

  // Start graphical interface loop and clear screen after exit
  screen.Loop(terminal);
  screen.ResetPosition(true);

  return EXIT_SUCCESS;
}
