#include "controller/player.h"

#include <string>

#include "error_table.h"
#include "model/wave.h"

namespace controller {

Player::Player(const std::shared_ptr<interface::EventDispatcher>& d)
    : interface::ActionListener(), dispatcher_(d), curr_song_(nullptr) {}

/* ********************************************************************************************** */

void Player::NotifyFileSelection(const std::filesystem::path& file) {
  auto result = Load(file);
  if (result == error::kSuccess) {
    // TODO: steps
    // notify blocks
    // send to alsa
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
    curr_song_ = std::make_unique<WaveFormat>();
    return curr_song_->ParseHeaderInfo(file.string());
  }

  return error::kFileNotSupported;
}

}  // namespace controller