

#include <cstdlib>  // for EXIT_SUCCESS

#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "view/base/terminal.h"                    // for Terminal

int main() {
  // Create and initialize a new player
  auto player = audio::Player::Create();

  // Create and initialize a new terminal window
  auto terminal = interface::Terminal::Create();

  // Create a full-size screen and register exit callback
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();

  // Register callbacks to Terminal
  terminal->RegisterPlayerControl(player);

  terminal->RegisterEventSenderCallback([&](ftxui::Event e) { screen.PostEvent(e); });

  terminal->RegisterExitCallback([&]() {
    player->Exit();
    screen.ExitLoopClosure()();
  });

  // Register callbacks to Player
  player->RegisterInterfaceNotifier(terminal->GetMediaController());

  // Start graphical interface loop
  screen.Loop(terminal);

  return EXIT_SUCCESS;
}