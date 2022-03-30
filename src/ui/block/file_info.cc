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

  return window(text(" Information "),
                vbox({vbox(std::move(elements)) | frame | xflex | size(HEIGHT, LESS_THAN, 15)}));
}

/* ********************************************************************************************** */

bool FileInfo::OnEvent(Event event) { return false; }

/* ********************************************************************************************** */

void FileInfo::ReadMusicFile(std::string path) { file_->ParseFromFile(SONG_PATH_FOR_DEV); }

}  // namespace interface
