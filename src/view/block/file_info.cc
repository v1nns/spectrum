#include "view/block/file_info.h"

#include <string>
#include <utility>  // for move
#include <vector>   // for vector

#include "ftxui/component/event.hpp"  // for Event

namespace interface {

constexpr int kMaxRows = 15;  //!< Maximum rows for the Component

/* ********************************************************************************************** */

FileInfo::FileInfo(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, kBlockFileInfo}, audio_info_{std::nullopt} {}

/* ********************************************************************************************** */

ftxui::Element FileInfo::Render() {
  ftxui::Elements lines;

  if (!audio_info_) {
    lines.push_back(ftxui::text("Choose a song to play...") | ftxui::dim);
  } else {
    std::istringstream input{model::to_string(audio_info_.value())};

    for (std::string line; std::getline(input, line);) {
      //   ftxui::Element line = ftxui::hbox({
      //       ftxui::text(title) | ftxui::bold | ftxui::color(ftxui::Color::CadetBlue),
      //       ftxui::filler(),
      //       ftxui::text(value) | ftxui::align_right,
      //   });

      // Create element
      ftxui::Element item = ftxui::text(line);
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

bool FileInfo::OnEvent(ftxui::Event event) {
  std::string parse = event.input();

  if (parse.find("evento|") != std::string::npos) {
    parse.erase(0, 7);
    std::stringstream ss(parse);
    model::Song teste;
    ss >> teste;
    audio_info_ = std::move(teste);
    return true;
  }

  return false;
}

}  // namespace interface
