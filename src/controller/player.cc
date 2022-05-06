#include "controller/player.h"

#include <string>

#include "model/application_error.h"
#include "model/song.h"
#include "model/wave.h"
#include "view/base/block_event.h"

namespace controller {

Player::Player(const std::shared_ptr<interface::EventDispatcher>& d)
    : interface::ActionListener(), dispatcher_(d), curr_song_(nullptr) {}

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

    // Notify all blocks with this event
    dispatcher->Broadcast(nullptr, event);

  } else {
    // Show error to user
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

}  // namespace controller