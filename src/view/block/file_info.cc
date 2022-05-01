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
    : Block(d, kBlockFileInfo), audio_info_() {}

/* ********************************************************************************************** */

Element FileInfo::Render() {
  Elements content;

  if (audio_info_.empty()) {
    content.push_back(text("No song has been chosen yet...") | dim);
  } else {
    // TODO: improve this... This will split the string into multiple lines using "." as delimiter
    std::string::size_type prev_pos = 0, pos = 0;

    while ((pos = audio_info_.find(".", pos)) != std::string::npos) {
      // get substring to print into a whole line
      std::string substring(audio_info_.substr(prev_pos, pos - prev_pos));

      // split into two strings to apply styles in the first one
      std::string::size_type token_index = substring.find(":");
      Element line = hbox({
          text(substring.substr(0, token_index)) | bold | color(Color::CadetBlue),
          text(substring.substr(token_index)),
      });

      content.push_back(line);
      prev_pos = ++pos;
    }
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
  if (event == BlockEvent::UpdateFileInfo) {
    // fill inner data
    audio_info_ = event.Content();
  }
}

}  // namespace interface
