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
  auto result = Load(file);

  // TODO: steps
  // notify blocks [X]
  // send to alsa  [ ]
  if (result == error::kSuccess) {
    // Create a block event
    auto event = interface::BlockEvent::UpdateFileInfo;
    std::string audio_info = to_string(curr_song_->GetAudioInformation());
    event.SetContent(audio_info);

    // Notify all blocks with this event
    if (auto d = dispatcher_.lock()) {
      d->Broadcast(nullptr, event);
    }  // TODO: or else?
  } else {
    // TODO: improve this if-else
    // Show error to user
    if (auto d = dispatcher_.lock()) {
      d->SetApplicationError(result);
    }
  }
}

/* ********************************************************************************************** */

bool Player::IsExtensionSupported(const std::filesystem::path& file) {
  bool supported = false;

  // TODO: in the future, create a map or some other type of structure to hold supported extensions
  if (file.extension() == ".wav") {
    supported = true;
  }

  return supported;
}

/* ********************************************************************************************** */

error::Value Player::Load(const std::filesystem::path& file) {
  if (IsExtensionSupported(file)) {
    curr_song_ = std::make_unique<model::WaveFormat>();
    return curr_song_->ParseHeaderInfo(file.string());
  }

  return error::kFileNotSupported;
}

}  // namespace controller