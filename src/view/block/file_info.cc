#include "view/block/file_info.h"

#include <utility>  // for move
#include <vector>   // for vector

#include "ftxui/component/event.hpp"  // for Event
#include "model/wave.h"               // for WaveFormat

namespace interface {

constexpr int kMaxRows = 15;  //!< Maximum rows for the Component

/* ********************************************************************************************** */

FileInfo::FileInfo(const std::shared_ptr<EventDispatcher>& d)
    : Block{d, kBlockFileInfo}, audio_info_{} {}

/* ********************************************************************************************** */

ftxui::Element FileInfo::Render() {
  ftxui::Elements lines;

  if (audio_info_.empty()) {
    lines.push_back(ftxui::text("Choose a song to play...") | ftxui::dim);
  } else {
    // TODO: improve this... This will split the string into multiple lines using "." as delimiter
    std::string::size_type prev_pos = 0, pos = 0;

    while ((pos = audio_info_.find(".", pos)) != std::string::npos) {
      // Get substring to print into a whole line
      std::string substring(audio_info_.substr(prev_pos, pos - prev_pos));

      // Split into two strings to apply styles in the first one
      std::string::size_type token_index = substring.find(":");
      std::string title = substring.substr(0, token_index),
                  value = substring.substr(token_index + 1);

      // Create element
      ftxui::Element line = ftxui::hbox({
          ftxui::text(title) | ftxui::bold | ftxui::color(ftxui::Color::CadetBlue),
          ftxui::filler(),
          ftxui::text(value) | ftxui::align_right,
      });

      lines.push_back(line);
      prev_pos = ++pos;
    }
  }

  ftxui::Element content = ftxui::vbox(lines);
  if (lines.size() == 1) content = content | ftxui::center;

  using ftxui::HEIGHT, ftxui::EQUAL;
  return ftxui::window(ftxui::text(" information "), std::move(content)) |
         ftxui::size(HEIGHT, EQUAL, kMaxRows);
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
