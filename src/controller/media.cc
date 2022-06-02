#include "controller/media.h"

#include <string>

#include "model/application_error.h"
#include "model/song.h"
#include "model/wave.h"
#include "view/base/block.h"
#include "view/base/block_event.h"

namespace controller {

Media::Media(const std::shared_ptr<interface::EventDispatcher>& d) : interface::ActionListener {
}, dispatcher_{d} {
}

/* ********************************************************************************************** */

void Media::NotifyFileSelection(const std::filesystem::path& file) {
  error::Code result = Load(file);
  auto dispatcher = dispatcher_.lock();

  // TODO: do something?
  if (!dispatcher) return;

  // TODO: steps
  // notify blocks [X]
  // send to alsa  [ ]
  //   if (result == error::kSuccess) {
  //     // Create a block event
  //     auto event = interface::BlockEvent::UpdateFileInfo;
  //     std::string audio_info = to_string(curr_song_->GetAudioInformation());
  //     event.SetContent(audio_info);

  //     // Notify File Info block with information about the recently loaded song
  //     dispatcher->SendTo(interface::kBlockFileInfo, event);

  //     // TODO: Still not working, need to think about it
  //     // // Clear previous song
  //     // if (loop_.joinable()) {
  //     //   stop_.store(true);
  //     //   loop_.join();
  //     //   stop_.store(false);
  //     // }

  //     // // Start thread to play song
  //     // loop_ = std::thread([&]() { this->PlaySong(); });

  //   } else {
  //     // Show error to user
  //     dispatcher->SetApplicationError(result);
  //   }
}

/* ********************************************************************************************** */

void Media::ClearCurrentSong() {
  error::Code result = Clear();
  auto dispatcher = dispatcher_.lock();

  // TODO: do something?
  if (!dispatcher) return;

  // Notify File Info block to reset its interface
  auto event = interface::BlockEvent::UpdateFileInfo;
  dispatcher->SendTo(interface::kBlockFileInfo, event);

  // TODO: Notify Audio Media block

  if (result != error::kSuccess) {
    dispatcher->SetApplicationError(result);
  }
}

/* ********************************************************************************************** */

error::Code Media::Load(const std::filesystem::path& file) {
  auto result = error::kFileNotSupported;

  // supported file extensions
  if (file.extension() != ".wav") {
    return result;
  }

  std::unique_ptr<model::Song> song = std::make_unique<model::WaveFormat>(file.string());
  result = song->ParseHeaderInfo();

  // release resources if got any error while trying to loading or do nothing?
  //   if (result == error::kSuccess) {
  // Alright, got a working header info, now load the whole data samples from song
  // curr_song_ = std::move(song);
  //   }

  return result;
}

/* ********************************************************************************************** */

error::Code Media::Clear() { return error::kUnknownError; }

}  // namespace controller