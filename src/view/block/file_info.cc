#include "view/block/file_info.h"

#include <string>
#include <string_view>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"
#include "view/element/style.h"
#include "view/element/util.h"

namespace interface {

FileInfo::FileInfo(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, model::BlockIdentifier::FileInfo,
            interface::Size{.width = 0, .height = kMaxRows}},
      audio_info_(kMaxSongLines) {
  // Fill with default content
  ParseAudioInfo(model::Song{});
  ParseAudioOutput(std::nullopt);
}

/* ********************************************************************************************** */

ftxui::Element FileInfo::Render() {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::LESS_THAN;
  using ftxui::WIDTH;

  // Audio output is shown right after song information
  std::vector<Entry> entries{audio_info_};
  entries.insert(entries.end(), output_info_.begin(), output_info_.end());

  ftxui::Elements lines;
  lines.reserve(entries.size());

  // Choose a different color for when there is no current song (paused song still has its info)
  const auto& theme = GetTheme().file_info;
  const ftxui::Color& color = has_song_info_ ? theme.value : theme.value_empty;

  for (const auto& [field, value] : entries) {
    // Calculate maximum width for text value (keeping a gap between field and value)
    const int width = kMaxColumns - static_cast<int>(field.size()) - kFieldGap;

    // Create element
    ftxui::Element item = ftxui::hbox({
        ftxui::text(field) | ftxui::bold | ftxui::color(theme.field),
        ftxui::filler(),
        // Long values are cut with an ellipsis (instead of animated), as the full filename is
        // already animated in files list when selected
        ftxui::text(ellipsize(value, width)) | ftxui::align_right |
            ftxui::size(WIDTH, LESS_THAN, width) | ftxui::color(color),
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
    ParseAudioOutput(std::nullopt);
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateAudioOutput) {
    LOG("Received audio output from player");
    ParseAudioOutput(event.GetContent<model::AudioOutput>());
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
  has_song_info_ = !audio.IsEmpty();

  // Use istringstream to split string into lines and parse it as <Field, Value>
  std::istringstream input{model::to_string(audio)};

  for (std::string line; std::getline(input, line);) {
    size_t pos = line.find_first_of(':');
    const std::string field = line.substr(0, pos);

    // Remove spaces around value (e.g. after ':'), so they do not take any column when rendered
    const std::string value = util::trim(line.substr(pos + 1));

    audio_info_.push_back({field, value});
  }
}

/* ********************************************************************************************** */

void FileInfo::ParseAudioOutput(const std::optional<model::AudioOutput>& output) {
  static constexpr std::string_view kEmpty = "<Empty>";

  // Format of audio samples sent to output device (e.g. "96 kHz / 32 bits")
  const std::string format =
      output ? util::format_with_prefix(output->format.sample_rate, "Hz") + " / " +
                   util::format_with_prefix(output->format.GetBitDepth(), "bits")
             : std::string{kEmpty};

  output_info_ = {
      {"Output", format},
      {"Device", output ? output->device : std::string{kEmpty}},
  };
}

}  // namespace interface
