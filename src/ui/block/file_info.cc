#include "ui/block/file_info.h"

#include "sound/wave.h"

namespace interface {

// TODO: remove this
#define SONG_PATH_FOR_DEV "/home/vinicius/projects/music-analyzer/africa-toto.wav"

/* ********************************************************************************************** */

FileInfo::FileInfo() { file_ = std::make_unique<WaveFormat>(); }

/* ********************************************************************************************** */

Element FileInfo::Render() {
  Elements elements;

  const auto lines = file_->GetFormattedStats();
  for (const auto& line : lines) {
    elements.push_back(text(line));
  }

  return window(text(" Files "), vbox({
                                     vbox(std::move(elements)) | frame | flex,
                                 })) |
         xflex | size(HEIGHT, EQUAL, 15);
}

/* ********************************************************************************************** */

bool FileInfo::OnEvent(Event event) {
  if (event == Event::Character(' ')) {
    file_->ParseFromFile(SONG_PATH_FOR_DEV);
    return true;
  }
  return false;
}

}  // namespace interface
