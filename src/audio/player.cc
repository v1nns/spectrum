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
      media_control_{},
      curr_song_{},
      notifier_{} {}

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
  // This value is used to decide buffer size for the step that starts decoding song
  int period_size = playback_->GetPeriodSize();

  while (!media_control_.exit) {
    // Block this thread until UI informs us a song to play
    if (media_control_.WaitForPlay()) {
      // First, try to parse file (it may be or not a support file extension to decode)
      error::Code result = decoder_->OpenFile(curr_song_.get());

      // In case of error, reset media controls and notify terminal UI with error
      if (result != error::kSuccess) {
        ResetMediaControl(result);
        continue;  // we don't wanna keep in this loop anymore, so wait for next song!
      }

      {
        // Otherwise, it is a supported audio extension, send detailed audio information to UI
        auto media_notifier = notifier_.lock();
        if (media_notifier) media_notifier->NotifySongInformation(*curr_song_);
      }

      // Inform playback driver to be ready to play
      playback_->Prepare();

      // To keep decoding audio, return true in lambda function
      result = decoder_->Decode(period_size, [&](void* buffer, int buffer_size, int out_samples) {
        if (media_control_.stop || media_control_.exit) {
          playback_->Stop();
          return false;
        }

        if (media_control_.pause) {
          playback_->Pause();
          media_control_.WaitForResume();
          playback_->Prepare();
        }

        playback_->AudioCallback(buffer, buffer_size, out_samples);
        return true;
      });

      // Reached the end of song, originated from one of these situations:
      // 1. naturally; 2. forced to stop/exit by user; 3. error from decoding;
      ResetMediaControl(result);
    }
  }
}

/* ********************************************************************************************** */

void Player::ResetMediaControl(error::Code err_code) {
  media_control_.Reset();
  curr_song_.reset();

  auto media_notifier = notifier_.lock();
  if (!media_notifier) return;

  // Clear any song information from UI
  media_notifier->ClearSongInformation();

  // And in case of error, notify about it
  if (err_code != error::kSuccess) media_notifier->NotifyError(err_code);
}

/* ********************************************************************************************** */

void Player::RegisterInterfaceNotifier(
    const std::shared_ptr<interface::InterfaceNotifier>& notifier) {
  notifier_ = notifier;
}

/* ********************************************************************************************** */

void Player::Play(const std::string& filepath) {
  curr_song_ = std::make_unique<model::Song>(model::Song{.filepath = filepath});
  media_control_.play = true;
  media_control_.Notify();
}

/* ********************************************************************************************** */

void Player::PauseOrResume() {
  if (!media_control_.play) {
    return;
  }

  bool curr_value = media_control_.pause;

  media_control_.pause = !curr_value;

  if (curr_value) media_control_.Notify();
}

/* ********************************************************************************************** */

void Player::Stop() {
  if (!media_control_.play) {
    return;
  }

  media_control_.play = false;
  media_control_.pause = false;
  media_control_.stop = true;
  media_control_.Notify();

  curr_song_.reset();
}

/* ********************************************************************************************** */

void Player::Exit() {
  media_control_.exit = true;
  media_control_.Notify();
}

}  // namespace audio