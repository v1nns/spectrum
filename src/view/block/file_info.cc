#include "view/block/file_info.h"

#include <utility>  // for move
#include <vector>   // for vector

#include "ftxui/component/event.hpp"  // for Event
#include "model/wave.h"               // for WaveFormat

namespace interface {

// TODO: remove this
#define SONG_PATH_FOR_DEV "/home/vinicius/projects/music-analyzer/africa-toto.wav"

/* ********************************************************************************************** */

FileInfo::FileInfo(const std::shared_ptr<EventDispatcher>& d)
    : Block(d, kBlockFileInfo), file_(nullptr) {}

/* ********************************************************************************************** */

Element FileInfo::Render() {
  Elements content;
  content.push_back(text("No song has been chosen yet...") | dim);

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
    // send to controller event.Content()
  }
}

}  // namespace interface
