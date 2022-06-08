#include "audio/player.h"

#include <stdexcept>

#include "view/base/action_listener.h"

namespace audio {

std::shared_ptr<Player> Player::Create(bool synchronous) {
  // Do something similar as to what was done with Terminal class
  struct MakeSharedEnabler : public Player {};
  auto player = std::make_unique<MakeSharedEnabler>();

  // Initialize internal components
  player->Init(synchronous);

  return player;
}

/* ********************************************************************************************** */

Player::Player()
    : playback_{},
      audio_loop_{},
      mutex_{},
      cond_var_{},
      play_{},
      stop_{},
      exit_{},
      curr_song_{},
      notifier_{} {}

/* ********************************************************************************************** */

Player::~Player() {
  if (audio_loop_.joinable()) {
    audio_loop_.join();
  }
}

/* ********************************************************************************************** */

void Player::Init(bool synchronous) {
  playback_ = std::make_unique<driver::Alsa>();
  error::Code result = playback_->CreatePlaybackStream();

  if (result != error::kSuccess) {
    throw std::runtime_error("Could not initialize player");
  }

  if (!synchronous) {
    audio_loop_ = std::thread(&Player::AudioHandler, this);
  }
}

/* ********************************************************************************************** */

void Player::AudioHandler() {
  while (!exit_.load()) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto stop_waiting_if = [&] { return play_.load() || exit_.load(); };

    cond_var_.wait(lock, stop_waiting_if);

    if (play_.load()) {
      driver::Decoder decoder;
      error::Code result = decoder.OpenFile(curr_song_.get());

      if (result != error::kSuccess) {
        ResetMediaControl();
        continue;
      }

      int period_size;
      result = playback_->ConfigureParameters(period_size);

      if (result != error::kSuccess) {
        ResetMediaControl();
        continue;
      }

      auto media_notifier = notifier_.lock();
      if (media_notifier) {
        media_notifier->NotifySongInformation(*curr_song_);
      }

      result = decoder.Decode(period_size, [&](void* buffer, int buffer_size, int out_samples) {
        if (exit_.load() || stop_.load()) return false;

        playback_->AudioCallback(buffer, buffer_size, out_samples);
        return true;
      });

      // do something with result

      play_.store(false);
    }
  }
}

/* ********************************************************************************************** */

void Player::ResetMediaControl() {
  curr_song_.reset();
  play_.store(false);
  stop_.store(false);
}

/* ********************************************************************************************** */

void Player::RegisterInterfaceNotifier(const std::shared_ptr<interface::ActionListener>& notifier) {
  notifier_ = notifier;
}

/* ********************************************************************************************** */

void Player::Play(const std::string& filepath) {
  curr_song_ = std::make_unique<model::Song>(model::Song{.filepath = filepath});
  play_.store(true);
  cond_var_.notify_one();
}

/* ********************************************************************************************** */

void Player::Stop() {
  curr_song_.reset();
  play_.store(false);
  stop_.store(true);
}

/* ********************************************************************************************** */

void Player::Exit() {
  exit_.store(true);
  cond_var_.notify_one();
}

}  // namespace audio