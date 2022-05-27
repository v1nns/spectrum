#include "controller/player.h"

#include <string>

#include "model/application_error.h"
#include "model/song.h"
#include "model/wave.h"
#include "view/base/block.h"
#include "view/base/block_event.h"

namespace controller {

Player::Player(const std::shared_ptr<interface::EventDispatcher>& d)
    : interface::ActionListener(),
      dispatcher_(d),
      driver_(std::make_unique<driver::AlsaSound>()),
      curr_song_(nullptr) {
  // TODO: handle errors
  driver_->Initialize();
}

/* ********************************************************************************************** */

void Player::NotifyFileSelection(const std::filesystem::path& file) {
  error::Code result = Load(file);
  auto dispatcher = dispatcher_.lock();

  // TODO: do something?
  if (!dispatcher) return;

  // TODO: steps
  // notify blocks [X]
  // send to alsa  [ ]
  if (curr_song_ && result == error::kSuccess) {
    // Create a block event
    auto event = interface::BlockEvent::UpdateFileInfo;
    std::string audio_info = to_string(curr_song_->GetAudioInformation());
    event.SetContent(audio_info);

    // Notify File Info block with information about the recently loaded song
    dispatcher->SendTo(interface::kBlockFileInfo, event);

  } else {
    // Show error to user
    dispatcher->SetApplicationError(result);
  }
}

/* ********************************************************************************************** */

void Player::ClearCurrentSong() {
  error::Code result = ClearSong();
  auto dispatcher = dispatcher_.lock();

  // TODO: do something?
  if (!dispatcher) return;

  // Notify File Info block to reset its interface
  auto event = interface::BlockEvent::UpdateFileInfo;
  dispatcher->SendTo(interface::kBlockFileInfo, event);

  // TODO: Notify Audio Player block

  if (result != error::kSuccess) {
    dispatcher->SetApplicationError(result);
  }
}

/* ********************************************************************************************** */

error::Code Player::Load(const std::filesystem::path& file) {
  auto result = error::kFileNotSupported;
  std::unique_ptr<model::Song> song;

  // supported file extensions
  if (file.extension() == ".wav") {
    song = std::make_unique<model::WaveFormat>(file.string());
    result = song->ParseHeaderInfo();
  }

  // release resources if got any error while trying to loading or do nothing?
  if (result == error::kSuccess) curr_song_.reset(song.release());

  return result;
}

/* ********************************************************************************************** */

// TODO: implement
error::Code Player::PlaySong() {
  auto result = error::kCorruptedData;

  return result;
}

/* ********************************************************************************************** */

error::Code Player::ClearSong() {
  auto result = error::kSuccess;

  if (curr_song_) {
    // TODO: release anything with driver_
    // and set any error if necessary

    curr_song_.reset();
  }

  return result;
}

}  // namespace controller