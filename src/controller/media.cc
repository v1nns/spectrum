#include "controller/media.h"

#include <string>
#include <thread>

#include "ftxui/component/event.hpp"
#include "model/application_error.h"
#include "model/song.h"
#include "view/base/block.h"

namespace controller {

Media::Media(const std::shared_ptr<interface::EventDispatcher>& dispatcher)
    : interface::ActionListener(),
      interface::InterfaceNotifier(),
      dispatcher_{dispatcher},
      player_ctl_{} {}

/* ********************************************************************************************** */

void Media::RegisterPlayerControl(const std::shared_ptr<audio::AudioControl>& player) {
  player_ctl_ = player;
}

/* ********************************************************************************************** */

void Media::NotifyFileSelection(const std::filesystem::path& filepath) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Play(filepath);
}

/* ********************************************************************************************** */

void Media::ClearCurrentSong() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Stop();
}

/* ********************************************************************************************** */

void Media::PauseOrResume() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->PauseOrResume();
}

/* ********************************************************************************************** */

void Media::ClearSongInformation() {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::ClearSongInfo();

  // Notify File Info block with to clear info about song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void Media::NotifySongInformation(const model::Song& info) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongInfo(info);

  // Notify File Info block with information about the recently loaded song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void Media::NotifySongState(const model::Song::State& state) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  auto event = interface::CustomEvent::UpdateSongState(state);

  // Notify Audio Player block with new state information about the current song
  dispatcher->SendEvent(event);
}

/* ********************************************************************************************** */

void Media::NotifyError(error::Code code) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  // Notify Terminal about error that has occurred in Audio thread
  dispatcher->SetApplicationError(code);
}

}  // namespace controller