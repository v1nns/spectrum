#include "audio/player.h"

namespace audio {

std::unique_ptr<Player> Player::Create(std::shared_ptr<model::GlobalResource> shared,
                                       bool synchronous) {
  // Do something similar as to what was done with Terminal class
  struct MakeUniqueEnabler : public Player {};
  auto player = std::make_unique<MakeUniqueEnabler>();

  // Initialize internal components
  player->Init(shared, synchronous);

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

void Player::Init(std::shared_ptr<model::GlobalResource> shared, bool synchronous) {
  shared_data_ = std::move(shared);

  driver_ = std::make_unique<driver::Alsa>();
  error::Code result = driver_->Initialize();

  if (result != error::kSuccess) {
    shared_data_->exit.store(true);
    return;  // TODO: improve this
  }

  if (!synchronous) {
    audio_loop_ = std::thread(&Player::AudioHandler, this);
  }
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
    auto stop_waiting_if = [&] { return shared_data_->play.load() || shared_data_->exit.load(); };

    shared_data_->cond_var.wait(lock, stop_waiting_if);

    if (shared_data_->play.load()) {
      model::AudioData audio_info = shared_data_->curr_song->GetAudioInformation();
      error::Code result = driver_->SetupAudioParameters(audio_info);

      if (result != error::kSuccess) {
      }

      result = driver_->Prepare();

      if (result != error::kSuccess) {
      }

      auto samples = shared_data_->curr_song->ParseData();
      driver_->Play(samples);

      return;
    }
  }
}

}  // namespace audio