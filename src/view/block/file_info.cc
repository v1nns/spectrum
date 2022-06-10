#include "view/block/file_info.h"

#include <string>
#include <utility>  // for move
#include <vector>   // for vector

#include "ftxui/component/event.hpp"  // for Event

namespace interface {

constexpr int kMaxRows = 15;  //!< Maximum rows for the Component

/* ********************************************************************************************** */

FileInfo::FileInfo(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::FileInfo}, audio_info_{std::nullopt} {}

/* ********************************************************************************************** */

ftxui::Element FileInfo::Render() {
  ftxui::Elements lines;

  if (!audio_info_) {
    lines.push_back(ftxui::text("Choose a song to play...") | ftxui::dim);
  } else {
    std::istringstream input{model::to_string(audio_info_.value())};
    size_t pos;

    for (std::string line; std::getline(input, line);) {
      pos = line.find_first_of(':');
      std::string field = line.substr(0, pos), value = line.substr(pos + 1);

      // Create element
      ftxui::Element item = ftxui::hbox({
          ftxui::text(field) | ftxui::bold | ftxui::color(ftxui::Color::CadetBlue),
          ftxui::filler(),
          ftxui::text(value) | ftxui::align_right,
      });

      lines.push_back(item);
    }
  }

  ftxui::Element content = ftxui::vbox(std::move(lines));
  if (lines.size() == 1) content = content | ftxui::center;

  using ftxui::HEIGHT, ftxui::EQUAL;
  return ftxui::window(ftxui::text(" information "), std::move(content)) |
         ftxui::size(HEIGHT, EQUAL, kMaxRows);
}

/* ********************************************************************************************** */

bool FileInfo::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

bool FileInfo::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Type::UpdateFileInfo) {
    audio_info_ = event.GetContent<model::Song>();
    return true;
  }

  if (event == CustomEvent::Type::ClearFileInfo) {
    audio_info_.reset();
    return true;
  }

  return false;
}

}  // namespace interface
