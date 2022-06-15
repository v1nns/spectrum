#include "audio/player.h"

#include <stdexcept>

#include "audio/driver/alsa.h"
#include "audio/driver/ffmpeg.h"
#include "view/base/interface_notifier.h"

namespace audio {

std::shared_ptr<Player> Player::Create(driver::Playback* playback, driver::Decoder* decoder,
                                       bool asynchronous) {
  // Create playback object
  auto pb = playback != nullptr ? std::unique_ptr<driver::Playback>(std::move(playback))
                                : std::make_unique<driver::Alsa>();

  // Create decoder object
  auto dec = decoder != nullptr ? std::unique_ptr<driver::Decoder>(std::move(decoder))
                                : std::make_unique<driver::FFmpeg>();

  // Instantiate Player
  auto player = std::shared_ptr<Player>(new Player(std::move(pb), std::move(dec)));

  // Initialize internal components
  player->Init(asynchronous);

  return player;
}

/* ********************************************************************************************** */

Player::Player(std::unique_ptr<driver::Playback>&& playback,
               std::unique_ptr<driver::Decoder>&& decoder)
    : playback_{std::move(playback)},
      decoder_{std::move(decoder)},
      audio_loop_{},
      play_{},
      pause_{},
      stop_{},
      exit_{},
      curr_song_{},
      notifier_{} {
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

void Player::Init(bool asynchronous) {
  // Open playback stream using default device
  error::Code result = playback_->CreatePlaybackStream();

  if (result != error::kSuccess) {
    throw std::runtime_error("Could not initialize playback stream in player");
  }

  // Configure desired parameters for playback
  result = playback_->ConfigureParameters();

  if (result != error::kSuccess) {
    throw std::runtime_error("Could not set parameters in player");
  }

  if (asynchronous) {
    // Spawn thread for Audio player
    audio_loop_ = std::thread(&Player::AudioHandler, this);
  }
}

/* ********************************************************************************************** */

void Player::AudioHandler() {
  int period_size = playback_->GetPeriodSize();

  while (!exit_) {
    // Block thread until UI informs us a song to play
    if (play_.WaitForSync()) {
      // First, try to parse file (it may be or not a support file extension to decode)
      error::Code result = decoder_->OpenFile(curr_song_.get());

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
      result = decoder_->Decode(period_size, [&](void* buffer, int buffer_size, int out_samples) {
        if (exit_ || stop_) {
          return false;
        }

        if (pause_) {
          playback_->Pause();
          pause_.WaitForSync();
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