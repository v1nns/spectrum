#include "controller/player.h"

#include <string>

#include "model/application_error.h"
#include "model/song.h"
#include "model/wave.h"
#include "view/base/block.h"
#include "view/base/block_event.h"

namespace controller {

Player::Player(const std::shared_ptr<interface::EventDispatcher>& d)
    : interface::ActionListener(),
      dispatcher_(d),
      mutex_(),
      stop_(false),
      //   loop_(),
      driver_(std::make_unique<driver::AlsaSound>()),
      curr_song_(nullptr) {
  // TODO: handle errors
  driver_->Initialize();
}

/* ********************************************************************************************** */

Player::~Player() {
  //   if (loop_.joinable()) {
  //     loop_.join();
  //   }
}

/* ********************************************************************************************** */

void Player::NotifyFileSelection(const std::filesystem::path& file) {
  error::Code result = Load(file);
  auto dispatcher = dispatcher_.lock();

  // TODO: do something?
  if (!dispatcher) return;

  // TODO: steps
  // notify blocks [X]
  // send to alsa  [ ]
  if (curr_song_ && result == error::kSuccess) {
    // Create a block event
    auto event = interface::BlockEvent::UpdateFileInfo;
    std::string audio_info = to_string(curr_song_->GetAudioInformation());
    event.SetContent(audio_info);

    // Notify File Info block with information about the recently loaded song
    dispatcher->SendTo(interface::kBlockFileInfo, event);

    // TODO: Still not working, need to think about it
    // // Clear previous song
    // if (loop_.joinable()) {
    //   stop_.store(true);
    //   loop_.join();
    //   stop_.store(false);
    // }

    // // Start thread to play song
    // loop_ = std::thread([&]() { this->PlaySong(); });

  } else {
    // Show error to user
    dispatcher->SetApplicationError(result);
  }
}

/* ********************************************************************************************** */

void Player::ClearCurrentSong() {
  error::Code result = Clear();
  auto dispatcher = dispatcher_.lock();

  // TODO: do something?
  if (!dispatcher) return;

  // Notify File Info block to reset its interface
  auto event = interface::BlockEvent::UpdateFileInfo;
  dispatcher->SendTo(interface::kBlockFileInfo, event);

  // TODO: Notify Audio Player block

  if (result != error::kSuccess) {
    dispatcher->SetApplicationError(result);
  }
}

/* ********************************************************************************************** */

error::Code Player::Load(const std::filesystem::path& file) {
  auto result = error::kFileNotSupported;

  // supported file extensions
  if (file.extension() != ".wav") {
    return result;
  }

  std::unique_ptr<model::Song> song = std::make_unique<model::WaveFormat>(file.string());
  result = song->ParseHeaderInfo();

  // release resources if got any error while trying to loading or do nothing?
  if (result == error::kSuccess) {
    // Alright, got a working header info, now load the whole data samples from song
    curr_song_ = std::move(song);
  }

  return result;
}

/* ********************************************************************************************** */

// TODO: implement
void Player::PlaySong() {
  error::Code result = error::kSuccess;
  model::AudioData audio_info;

  {
    std::scoped_lock lk{mutex_};
    audio_info = curr_song_->GetAudioInformation();
    result = driver_->SetupAudioParameters(audio_info);
  }

  if (result != error::kSuccess) return;

  const std::vector<double> song_data(curr_song_->ParseData());

  result = driver_->Prepare();
  if (result != error::kSuccess) return;

  while (!stop_.load()) {
    result = driver_->Play(song_data);
    if (result != error::kSuccess) {
      driver_->Stop();
      return;
    }
  }
}

/* ********************************************************************************************** */

error::Code Player::Clear() {
  auto result = error::kSuccess;

  if (curr_song_) {
    // TODO: release anything with driver_
    // and set any error if necessary

    curr_song_.reset();
  }

  return result;
}

}  // namespace controller