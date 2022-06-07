

#include <cstdlib>  // for EXIT_SUCCESS
#include <memory>   // for make_unique, unique_ptr

#include "audio/driver/alsa.h"                     // remove
#include "audio/driver/decoder.h"                  // remove
#include "audio/player.h"                          // for Player
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "model/global_resource.h"
#include "model/song.h"          // remove
#include "view/base/terminal.h"  // for Terminal

int main() {
  std::string path = "/home/vinicius/projects/music-analyzer/soulbreeder.wav";

  driver::Decoder decoder;
  auto result = decoder.OpenFile(path);

  std::cout << "decoder.OpenFile() retornou isso aqui: " << result << std::endl;
  if (result != error::kSuccess) return EXIT_FAILURE;

  driver::Alsa playback;
  result = playback.CreatePlaybackStream();

  std::cout << "playback.CreatePlaybackStream() retornou isso aqui: " << result << std::endl;
  if (result != error::kSuccess) return EXIT_FAILURE;

  int samples;
  playback.ConfigureParameters(samples);

  std::cout << "playback.ConfigureParameters() retornou isso aqui: " << result << std::endl;
  if (result != error::kSuccess) return EXIT_FAILURE;

  result = decoder.Decode(samples, [&](void *buffer, int buffer_size, int out_samples) {
    playback.AudioCallback(buffer, buffer_size, out_samples);
  });

  std::cout << "decoder.Decode() retornou isso aqui: " << result << std::endl;

  return EXIT_SUCCESS;
}

/* ********************************************************************************************** */

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