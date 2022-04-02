#include "ui/block/file_info.h"

#include "ftxui/component/event.hpp"  // for Event, Event::ArrowDown, Event:...
#include "sound/wave.h"

namespace interface {

// TODO: remove this
#define SONG_PATH_FOR_DEV "/home/vinicius/projects/music-analyzer/africa-toto.wav"

/* ********************************************************************************************** */

FileInfo::FileInfo(const std::shared_ptr<Dispatcher>& d)
    : Block(d, kBlockFileInfo), file_(nullptr) {}

/* ********************************************************************************************** */

Element FileInfo::Render() {
  Elements content;

  if (file_) {
    const auto lines = file_->GetFormattedStats();
    for (const auto& line : lines) {
      content.push_back(text(line));
    }
  } else {
    content.push_back(text("No song has been chosen yet...") | dim);
  }

  return window(text(" Information "),
                vbox({
                    vbox(std::move(content)) | frame | xflex | size(HEIGHT, EQUAL, 15),
                }));
}

/* ********************************************************************************************** */

bool FileInfo::OnEvent(Event event) { return false; }

/* ********************************************************************************************** */

void FileInfo::OnBlockEvent(BlockEvent event) {
  if (event == BlockEvent::FileSelected) {
    ReadMusicFile("");  // get content from event
  }
}

/* ********************************************************************************************** */

void FileInfo::ReadMusicFile(std::string path) {
  file_ = std::make_unique<WaveFormat>();
  file_->ParseFromFile(SONG_PATH_FOR_DEV);
}

}  // namespace interface
