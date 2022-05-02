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

ftxui::Element FileInfo::Render() {
  ftxui::Elements content;

  if (audio_info_.empty()) {
    content.push_back(ftxui::text("No song has been chosen yet...") | ftxui::dim);
  } else {
    // TODO: improve this... This will split the string into multiple lines using "." as delimiter
    std::string::size_type prev_pos = 0, pos = 0;

    while ((pos = audio_info_.find(".", pos)) != std::string::npos) {
      // get substring to print into a whole line
      std::string substring(audio_info_.substr(prev_pos, pos - prev_pos));

      // split into two strings to apply styles in the first one
      std::string::size_type token_index = substring.find(":");
      ftxui::Element line = ftxui::hbox({
          ftxui::text(substring.substr(0, token_index)) | ftxui::bold |
              ftxui::color(ftxui::Color::CadetBlue),
          ftxui::text(substring.substr(token_index)),
      });

      content.push_back(line);
      prev_pos = ++pos;
    }
  }

  using ftxui::HEIGHT, ftxui::EQUAL;
  return ftxui::window(ftxui::text(" Information "),
                       ftxui::vbox({
                           ftxui::vbox(std::move(content)) | ftxui::frame | ftxui::xflex |
                               ftxui::size(HEIGHT, EQUAL, 15),
                       }));
}

/* ********************************************************************************************** */

bool FileInfo::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

void FileInfo::OnBlockEvent(BlockEvent event) {
  if (event == BlockEvent::UpdateFileInfo) {
    // fill inner data
    audio_info_ = event.Content();
  }
}

}  // namespace interface
