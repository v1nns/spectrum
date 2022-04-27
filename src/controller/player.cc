#include "controller/player.h"

#include <string>

#include "error_table.h"

Player::Player(const std::shared_ptr<interface::Dispatcher>& d)
    : dispatcher_(d), curr_song_(nullptr) {}

/* ********************************************************************************************** */

void Player::UserSelectedFile(){};

/* ********************************************************************************************** */

bool Player::IsFormatSupported(const std::filesystem::path& file) {
  bool supported = false;

  // TODO: in the future, create a map or some other type of structure to hold supported extensions
  if (file.extension() == ".wav") {
    supported = true;
  }

  return supported;
}

/* ********************************************************************************************** */

int Player::Load(const std::filesystem::path& file) { return error::kFileNotSupported; }