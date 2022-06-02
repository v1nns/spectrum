#include "audio/player.h"

#include <iostream>  // tmp

namespace audio {

std::unique_ptr<Player> Player::Create(std::shared_ptr<model::GlobalResource> shared) {
  // Do something similar as to what was done with Terminal class
  struct MakeUniqueEnabler : public Player {};
  auto player = std::make_unique<MakeUniqueEnabler>();

  // Initialize internal components
  player->Init(shared);

  return player;
}

/* ********************************************************************************************** */

Player::Player() : driver_{}, shared_data_{}, audio_loop_{} {}

/* ********************************************************************************************** */

Player::~Player() {
  if (audio_loop_.joinable()) {
    audio_loop_.join();
  }
}

/* ********************************************************************************************** */

void Player::Init(std::shared_ptr<model::GlobalResource> shared) {
  shared_data_ = std::move(shared);
  audio_loop_ = std::thread(&Player::AudioHandler, this);
}

/* ********************************************************************************************** */

void Player::Exit() {
  // Notify other threads to force an exit
  shared_data_->NotifyToExit();
}

/* ********************************************************************************************** */

void Player::AudioHandler() {
  while (!shared_data_->exit.load()) {
    std::unique_lock<std::mutex> lock(shared_data_->mutex);
    shared_data_->cond_var.wait(
        lock, [&] { return shared_data_->play.load() || shared_data_->exit.load(); });

    if (shared_data_->play.load()) std::cout << "yeahhhh started playing" << std::endl;
  }

  std::cout << "oh no, i'm dyingggggggg";
  if (!shared_data_->play.load()) std::cout << " without even playing";
  std::cout << std::endl;
}

}  // namespace audio