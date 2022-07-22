#include "middleware/media_controller.h"

#include <string>
#include <thread>

#include "audio/player.h"
#include "ftxui/component/event.hpp"
#include "model/application_error.h"
#include "model/song.h"
#include "view/base/block.h"
#include "view/base/terminal.h"

namespace middleware {

std::shared_ptr<MediaController> MediaController::Create(
    const std::shared_ptr<interface::Terminal>& terminal,
    const std::shared_ptr<audio::Player>& player) {
  auto controller = std::shared_ptr<MediaController>(new MediaController(terminal, player));

  // Register callbacks to Terminal
  terminal->RegisterInterfaceListener(controller);

  // Register callbacks to Player
  player->RegisterInterfaceNotifier(controller);

  return controller;
}

/* ********************************************************************************************** */

MediaController::MediaController(const std::shared_ptr<interface::EventDispatcher>& dispatcher,
                                 const std::shared_ptr<audio::AudioControl>& player_ctl)
    : interface::Listener(),
      interface::Notifier(),
      dispatcher_{dispatcher},
      player_ctl_{player_ctl},
      buffer_() {}

/* ********************************************************************************************** */

void MediaController::NotifyFileSelection(const std::filesystem::path& filepath) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Play(filepath);
}

/* ********************************************************************************************** */

void MediaController::ClearCurrentSong() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Stop();
}

/* ********************************************************************************************** */

void MediaController::PauseOrResume() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->PauseOrResume();
}

/* ********************************************************************************************** */

void MediaController::ClearSongInformation() {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::ClearSongInfo();

  // Notify File Info block with to clear info about song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::NotifySongInformation(const model::Song& info) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongInfo(info);

  // Notify File Info block with information about the recently loaded song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::NotifySongState(const model::Song::State& state) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongState(state);

  // Notify Audio Player block with new state information about the current song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void MediaController::SendAudioRaw(int* buffer, int buffer_size) {
  buffer_.insert(buffer_.end(), buffer, buffer + buffer_size);

  // 44100 * 0.3s = 13230
  if (buffer_.size() > 13230) {
    // auto dispatcher = dispatcher_.lock();
    // if (!dispatcher) return;
    //
    // auto event = interface::CustomEvent::DrawAudioRaw(buffer_.data(), buffer_.size());
    //
    // // Notify Audio Player block with new state information about the current song
    // dispatcher->SendEvent(event);

    // clear
    buffer_.clear();
  }
}

/* ********************************************************************************************** */

void MediaController::NotifyError(error::Code code) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  // Notify Terminal about error that has occurred in Audio thread
  dispatcher->SetApplicationError(code);
}

}  // namespace middleware
