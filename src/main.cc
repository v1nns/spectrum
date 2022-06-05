#include <chrono>   // remove
#include <cstdlib>  // for EXIT_SUCCESS
#include <memory>   // for make_unique, unique_ptr
#include <thread>   // remove

#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "model/global_resource.h"
#include "model/wave.h"          // remove
#include "view/base/terminal.h"  // for Terminal

int main() {
  auto shared_data = std::make_shared<model::GlobalResource>();

  // Create and initialize a new player
  auto player = audio::Player::Create(shared_data, true);

  auto producer = [shared_data]() {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);

    std::string path = "/home/vinicius/projects/music-analyzer/africa-toto.wav";
    std::unique_ptr<model::Song> song = std::make_unique<model::WaveFormat>(path);
    song->ParseHeaderInfo();

    {
      std::scoped_lock<std::mutex> lock{shared_data->mutex};
      shared_data->curr_song = std::move(song);
      shared_data->play.store(true);
      shared_data->cond_var.notify_one();
    }
  };

  auto thread = std::thread(producer);

  player->AudioHandler();

  thread.join();

  return EXIT_SUCCESS;
}

// int main() {
//   auto shared_data = std::make_shared<model::GlobalResource>();

//   // Create and initialize a new player
//   auto player = audio::Player::Create(shared_data);

//   // Create and initialize a new terminal window
//   auto terminal = interface::Terminal::Create(shared_data);

//   // Create a full-size screen and register exit callback
//   ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
//   terminal->RegisterExitCallback(screen.ExitLoopClosure());

//   // Start graphical interface loop
//   screen.Loop(terminal);

//   return EXIT_SUCCESS;
// }