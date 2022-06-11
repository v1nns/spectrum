#include "audio/player.h"

#include <stdexcept>

#include "view/base/interface_notifier.h"

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
    : playback_{}, audio_loop_{}, play_{}, pause_{}, stop_{}, exit_{}, curr_song_{}, notifier_{} {
  play_.wait_until = [&] { return play_ || exit_; };
  pause_.wait_until = [&] { return !pause_ || exit_; };
}

/* ********************************************************************************************** */

Player::~Player() {
  if (audio_loop_.joinable()) {
    audio_loop_.join();
  }
}

/* ********************************************************************************************** */

void Player::Init(bool synchronous) {
  // Create playback stream on ALSA
  playback_ = std::make_unique<driver::Alsa>();
  error::Code result = playback_->CreatePlaybackStream();

  if (result != error::kSuccess) {
    throw std::runtime_error("Could not initialize playback stream in player");
  }

  // Configure desired parameters for playback
  result = playback_->ConfigureParameters();

  if (result != error::kSuccess) {
    throw std::runtime_error("Could not set parameters in player");
  }

  // Spawn thread for Audio player
  if (!synchronous) {
    audio_loop_ = std::thread(&Player::AudioHandler, this);
  }
}

/* ********************************************************************************************** */

void Player::AudioHandler() {
  int period_size = playback_->GetPeriodSize();

  while (!exit_) {
    // Block thread until UI informs us a song to play
    if (play_.WaitForValue()) {
      // First, try to parse file (it may be or not a support file extension to decode)
      driver::Decoder decoder;
      error::Code result = decoder.OpenFile(curr_song_.get());

      // In case of error, reset media controls
      // TODO: and should inform UI about this with an application error
      if (result != error::kSuccess) {
        ResetMediaControl();
        continue;
      }

      {
        // Otherwise, it is a supported audio extension, send detailed audio information to UI
        auto media_notifier = notifier_.lock();
        if (media_notifier) {
          media_notifier->NotifySongInformation(*curr_song_);
        }
      }

      // Inform ALSA to be ready to play
      playback_->Prepare();

      // To keep decoding audio, return true in lambda function
      result = decoder.Decode(period_size, [&](void* buffer, int buffer_size, int out_samples) {
        if (exit_ || stop_) {
          return false;
        }

        if (pause_) {
          playback_->Pause();
          pause_.WaitForValue();
          playback_->Prepare();
        }

        playback_->AudioCallback(buffer, buffer_size, out_samples);
        return true;
      });

      // TODO: do something with result

      if (stop_) {
        playback_->Stop();
      }

      // Reached the end of song (naturally or forced by user)
      ResetMediaControl();
    }
  }
}

/* ********************************************************************************************** */

void Player::ResetMediaControl() {
  curr_song_.reset();
  play_ = false;
  stop_ = false;

  auto media_notifier = notifier_.lock();
  if (media_notifier) {
    media_notifier->ClearSongInformation();
  }
}

/* ********************************************************************************************** */

void Player::RegisterInterfaceNotifier(
    const std::shared_ptr<interface::InterfaceNotifier>& notifier) {
  notifier_ = notifier;
}

/* ********************************************************************************************** */

void Player::Play(const std::string& filepath) {
  curr_song_ = std::make_unique<model::Song>(model::Song{.filepath = filepath});
  play_ = true;
  play_.Notify();
}

/* ********************************************************************************************** */

void Player::PauseOrResume() {
  if (!play_) {
    return;
  }

  if (!pause_) {
    pause_ = true;
  } else {
    pause_ = false;
    pause_.Notify();
  }
}

/* ********************************************************************************************** */

void Player::Stop() {
  if (pause_) {
    pause_ = false;
    pause_.Notify();
  }

  curr_song_.reset();
  play_ = false;
  stop_ = true;
}

/* ********************************************************************************************** */

void Player::Exit() {
  exit_ = true;
  play_.Notify();
  pause_.Notify();
}

}  // namespace audio