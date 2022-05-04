#include "view/block/audio_player.h"

#include <utility>  // for move
#include <vector>   // for vector

#include "ftxui/component/event.hpp"  // for Event
#include "model/wave.h"               // for WaveFormat

namespace interface {

constexpr int kMaxRows = 10;  //!< Maximum rows for the Component

/* ********************************************************************************************** */

AudioPlayer::AudioPlayer(const std::shared_ptr<EventDispatcher>& d) : Block(d, kBlockAudioPlayer) {}

/* ********************************************************************************************** */

ftxui::Element AudioPlayer::Render() {
  ftxui::Elements content;

  using ftxui::HEIGHT, ftxui::EQUAL;
  return ftxui::window(ftxui::text(" player "), ftxui::vbox(std::move(content)) | ftxui::frame |
                                                    ftxui::xflex |
                                                    ftxui::size(HEIGHT, EQUAL, kMaxRows));
}

/* ********************************************************************************************** */

bool AudioPlayer::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

void AudioPlayer::OnBlockEvent(BlockEvent event) {
  // TODO: ...
}

}  // namespace interface
