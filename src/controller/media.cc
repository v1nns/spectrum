#include "controller/media.h"

#include <string>
#include <thread>

#include "model/application_error.h"
#include "model/song.h"
#include "model/wave.h"
#include "view/base/block.h"
#include "view/base/block_event.h"

namespace controller {

Media::Media(const std::shared_ptr<interface::EventDispatcher>& dispatcher,
             const std::shared_ptr<model::GlobalResource>& resource)
    : interface::ActionListener {
}, dispatcher_{dispatcher}, shared_data_{resource} {
}

/* ********************************************************************************************** */

void Media::NotifyFileSelection(const std::filesystem::path& filepath) {
  model::AudioData audio_info{};
  error::Code result = ReadFile(filepath, audio_info);
  auto dispatcher = dispatcher_.lock();

  // TODO: do something?
  if (!dispatcher) return;

  if (result == error::kSuccess) {
    // Create a block event
    auto event = interface::BlockEvent::UpdateFileInfo;
    std::string text = to_string(audio_info);
    event.SetContent(text);

    // Notify File Info block with information about the recently loaded song
    dispatcher->SendTo(interface::kBlockFileInfo, event);

  } else {
    // Show error to user
    dispatcher->SetApplicationError(result);
  }
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

  if (result != error::kSuccess) {
    dispatcher->SetApplicationError(result);
  }
}

/* ********************************************************************************************** */

error::Code Media::ReadFile(const std::filesystem::path& file, model::AudioData& info) {
  auto result = error::kFileNotSupported;

  // Supported file extensions
  if (file.extension() != ".wav") {
    return result;
  }

  std::unique_ptr<model::Song> song = std::make_unique<model::WaveFormat>(file.string());
  result = song->ParseHeaderInfo();

  // Alright, got a working header info, save it to global resources
  if (result == error::kSuccess) {
    // Get pointer to shared resources
    auto resource = shared_data_.lock();
    if (!resource) return error::kUnknownError;

    // Fill auxiliary struct from function parameters
    info = song->GetAudioInformation();

    {
      // Save song in shared data and signal to start playing it
      std::scoped_lock<std::mutex> lock{resource->mutex};
      resource->curr_song = std::move(song);
      resource->play.store(true);
      resource->cond_var.notify_one();
    }
  }

  return result;
}

/* ********************************************************************************************** */

error::Code Media::Clear() {
  auto result = error::kUnknownError;
  auto resource = shared_data_.lock();

  if (!resource) return result;

  {
    std::scoped_lock<std::mutex> lock{resource->mutex};
    resource->curr_song.reset();
    resource->play.store(false);

    result = error::kSuccess;
  }

  return result;
}

}  // namespace controller