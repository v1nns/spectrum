#include "controller/media.h"

#include <string>
#include <thread>

#include "model/application_error.h"
#include "model/song.h"
#include "view/base/block.h"
#include "view/base/block_event.h"

namespace controller {

Media::Media(const std::shared_ptr<interface::EventDispatcher>& dispatcher)
    : interface::ActionListener {
}, dispatcher_{dispatcher}, player_ctl_{} {
}

/* ********************************************************************************************** */

void Media::RegisterPlayerControl(const std::shared_ptr<audio::PlayerControl>& player) {
  player_ctl_ = player;
}

/* ********************************************************************************************** */

void Media::NotifyFileSelection(const std::filesystem::path& filepath) {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Play(filepath);

  //   } else {
  //     // Show error to user
  //     dispatcher->SetApplicationError(result);
  //   }
}

/* ********************************************************************************************** */

void Media::ClearCurrentSong() {
  auto player = player_ctl_.lock();
  if (!player) return;

  player->Stop();

  //   auto dispatcher = dispatcher_.lock();
  //   if (!dispatcher) return;

  //   // Notify File Info block to reset its interface
  //   auto event = interface::BlockEvent::UpdateFileInfo;
  //   dispatcher->SendTo(interface::kBlockFileInfo, event);

  //   if (result != error::kSuccess) {
  //     dispatcher->SetApplicationError(result);
  //   }
}

/* ********************************************************************************************** */

void Media::NotifySongInformation(const model::Song& info) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  // Create a block event
  auto event = interface::BlockEvent::UpdateFileInfo;
  std::string text = to_string(info);
  event.SetContent(text);

  // Notify File Info block with information about the recently loaded song
  dispatcher->SendTo(interface::kBlockFileInfo, event);
}

}  // namespace controller