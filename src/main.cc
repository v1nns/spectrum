

#include <cstdlib>  // for EXIT_SUCCESS

#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "view/base/terminal.h"                    // for Terminal

void teste() {
  auto player = audio::Player::Create();

  player->Play("/home/vinicius/projects/music-analyzer/crazysong.wav");
  while (1) {
  }
}

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
    player->Exit();
    screen.ExitLoopClosure()();
  });

  // Start graphical interface loop
  screen.Loop(terminal);

  return EXIT_SUCCESS;
}