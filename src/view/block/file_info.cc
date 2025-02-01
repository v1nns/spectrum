#include "view/block/file_info.h"

#include <string>

#include "ftxui/component/event.hpp"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"

namespace interface {

FileInfo::FileInfo(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, model::BlockIdentifier::FileInfo,
            interface::Size{.width = 0, .height = kMaxRows}},
      audio_info_(kMaxSongLines) {
  // Fill with default content
  ParseAudioInfo(model::Song{});
}

/* ********************************************************************************************** */

ftxui::Element FileInfo::Render() {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::LESS_THAN;
  using ftxui::WIDTH;

  ftxui::Elements lines;
  lines.reserve(audio_info_.size());

  // Choose a different color for when there is no current song
  ftxui::Color::Palette256 color =
      is_song_playing_ ? ftxui::Color::LightSteelBlue1 : ftxui::Color::LightSteelBlue3;

  for (const auto& [field, value] : audio_info_) {
    // Calculate maximum width for text value
    int width = kMaxColumns - field.size();

    // Create element
    ftxui::Element item = ftxui::hbox({
        ftxui::text(field) | ftxui::bold | ftxui::color(ftxui::Color::SteelBlue1),
        ftxui::filler(),
        // TODO: maybe use TextAnimation element for Field filename
        ftxui::text(value) | ftxui::align_right | ftxui::size(WIDTH, LESS_THAN, width) |
            ftxui::color(ftxui::Color(color)),
    });

    lines.push_back(item);
  }

  ftxui::Element content = ftxui::vbox(lines);

  return ftxui::window(ftxui::hbox(ftxui::text(" information ") | GetTitleDecorator()), content) |
         ftxui::size(HEIGHT, EQUAL, kMaxRows) | GetBorderDecorator();
}

/* ********************************************************************************************** */

bool FileInfo::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

bool FileInfo::OnCustomEvent(const CustomEvent& event) {
  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    ParseAudioInfo(model::Song{});
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");
    ParseAudioInfo(event.GetContent<model::Song>());
  }

  return false;
}

/* ********************************************************************************************** */

void FileInfo::ParseAudioInfo(const model::Song& audio) {
  audio_info_.clear();
  is_song_playing_ = !audio.IsEmpty(); // TODO: evaluate this

  // Use istringstream to split string into lines and parse it as <Field, Value>
  std::istringstream input{model::to_string(audio)};

  for (std::string line; std::getline(input, line);) {
    size_t pos = line.find_first_of(':');
    std::string field = line.substr(0, pos), value = line.substr(pos + 1);

    audio_info_.push_back({field, value});
  }
}

}  // namespace interface
