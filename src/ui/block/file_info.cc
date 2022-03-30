#include "ui/block/file_info.h"

#include "sound/wave.h"

namespace interface {

// TODO: remove this
#define SONG_PATH_FOR_DEV "/home/vinicius/projects/music-analyzer/africa-toto.wav"

/* ********************************************************************************************** */

FileInfo::FileInfo() : file_(nullptr) {}

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

void FileInfo::ReadMusicFile(std::string path) {
  file_ = std::make_unique<WaveFormat>();
  file_->ParseFromFile(SONG_PATH_FOR_DEV);
}

}  // namespace interface
