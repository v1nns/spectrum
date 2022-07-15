/**
 * \file
 * \brief Main function
 */
#include <cstdlib>  // for EXIT_SUCCESS

#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
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

  terminal->RegisterExitCallback([&]() {
    screen.ExitLoopClosure()();
  });

  // Start graphical interface loop
  screen.Loop(terminal);

  // Clear screen and exit from player thread
  screen.Clear();
  player->Exit();

  return EXIT_SUCCESS;
}
