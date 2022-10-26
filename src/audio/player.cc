#include "audio/player.h"

#include <stdexcept>

#include "audio/driver/alsa.h"
#include "audio/driver/ffmpeg.h"
#include "util/logger.h"
#include "view/base/notifier.h"

namespace audio {

std::shared_ptr<Player> Player::Create(driver::Playback* playback, driver::Decoder* decoder,
                                       bool asynchronous) {
  LOG("Creating a new instance of Player");

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
      media_control_{.state = State::Idle},
      curr_song_{},
      notifier_{},
      period_size_() {}

/* ********************************************************************************************** */

Player::~Player() {
  Exit();

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

  // This value is used to decide buffer size for song decoding
  period_size_ = playback_->GetPeriodSize();

  if (asynchronous) {
    // Spawn thread for Audio player
    audio_loop_ = std::thread(&Player::AudioHandler, this);
  }
}

/* ********************************************************************************************** */

void Player::AudioHandler() {
  // Block this thread until UI informs us a song to play
  while (media_control_.WaitFor(Command::Play)) {
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

    int curr_position = -1;  // in seconds

    // To keep decoding audio, return true in lambda function
    result = decoder_->Decode(
        period_size_, [&](void* buffer, int max_size, int actual_size, int64_t& position) mutable {
          auto command = media_control_.Pop();
          auto media_notifier = notifier_.lock();

          switch (command) {
            case Command::PauseOrResume: {
              media_control_.state = TranslateCommand(command);
              playback_->Pause();

              // As this thread can stay blocked for a long time, waiting for a command,
              // notify state to media controller
              if (media_notifier) {
                media_notifier->NotifySongState(model::Song::CurrentInformation{
                    .state = model::Song::MediaState::Pause,
                    .position = (uint32_t)curr_position,
                });
              }

              // Block thread until receives one of the informed commands
              bool keep_executing = media_control_.WaitFor(Command::PauseOrResume, Command::Stop);

              // TODO: NotifySongState for stop

              if (!keep_executing || media_control_.state == State::Stop) {
                playback_->Stop();
                return false;
              }

              media_control_.state = State::Play;
              playback_->Prepare();
            } break;

            case Command::Stop:
            case Command::Exit: {
              media_control_.state = TranslateCommand(command);
              playback_->Stop();
              return false;
            } break;

            case Command::SeekForward: {
              if (position < curr_song_->duration) {
                position++;
                return true;
              }
            } break;

            case Command::SeekBackward: {
              if (position > 0) {
                position--;
                return true;
              }
            } break;

            default:
              break;
          }

          // Send raw information to media controller to run audio analysis
          if (media_notifier) {
            media_notifier->SendAudioRaw((int*)buffer, actual_size);
          }

          // Write samples to playback
          playback_->AudioCallback(buffer, max_size, actual_size);

          // Notify song state to graphical interface
          if (position != curr_position) {
            curr_position = position;

            if (media_notifier) {
              media_notifier->NotifySongState(model::Song::CurrentInformation{
                  .state = model::Song::MediaState::Play,
                  .position = (uint32_t)curr_position,
              });
            }
          }

          return true;
        });

    // Reached the end of song, originated from one of these situations:
    // 1. naturally; 2. forced to stop/exit by user; 3. error from decoding;
    ResetMediaControl(result);
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

void Player::RegisterInterfaceNotifier(const std::shared_ptr<interface::Notifier>& notifier) {
  notifier_ = notifier;
}

/* ********************************************************************************************** */

void Player::Play(const std::string& filepath) {
  curr_song_ = std::make_unique<model::Song>(model::Song{.filepath = filepath});
  media_control_.Push(Command::Play);
}

/* ********************************************************************************************** */

void Player::PauseOrResume() { media_control_.Push(Command::PauseOrResume); }

/* ********************************************************************************************** */

void Player::Stop() { media_control_.Push(Command::Stop); }

/* ********************************************************************************************** */

void Player::SetAudioVolume(model::Volume value) { playback_->SetVolume(value); }

/* ********************************************************************************************** */

model::Volume Player::GetAudioVolume() { return playback_->GetVolume(); }

/* ********************************************************************************************** */

void Player::SeekForwardPosition(int value) { media_control_.Push(Command::SeekForward); }

/* ********************************************************************************************** */

void Player::SeekBackwardPosition(int value) { media_control_.Push(Command::SeekBackward); }

/* ********************************************************************************************** */

void Player::Exit() { media_control_.Push(Command::Exit); }

}  // namespace audio
