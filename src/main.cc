

#include <cstdlib>  // for EXIT_SUCCESS

#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "view/base/terminal.h"                    // for Terminal

int main() {
  // Create and initialize a new player
  auto player = audio::Player::Create();

  // Create and initialize a new terminal window
  auto terminal = interface::Terminal::Create();

  // Register interfaces to each other
  terminal->RegisterPlayerControl(player);
  // TODO: fix this
  //   player->RegisterInterfaceNotifier(terminal->GetMediaController());

  // Create a full-size screen and register exit callback
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

  terminal->RegisterExitCallback([&]() {
    player->Exit();
    screen.ExitLoopClosure()();
  });

  // Start graphical interface loop
  screen.Loop(terminal);

  return EXIT_SUCCESS;
}