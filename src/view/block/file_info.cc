#include "view/block/file_info.h"

#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"
#include "view/element/util.h"

namespace interface {

namespace {

//! Separator between values shown on the same line
constexpr std::string_view kSeparator = " · ";

//! Get user-friendly text for number of channels
std::string ChannelsToString(uint16_t channels) {
  static constexpr uint16_t kMono = 1;
  static constexpr uint16_t kStereo = 2;

  switch (channels) {
    case kMono:
      return "mono";
    case kStereo:
      return "stereo";
    default:
      return std::to_string(channels) + " channels";
  }
}

}  // namespace

/* ********************************************************************************************** */

FileInfo::FileInfo(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, model::BlockIdentifier::FileInfo,
            interface::Size{.width = 0, .height = kMaxRows}} {
  // Fill with default content
  ParseAudioInfo(model::Song{});
  ParseAudioOutput(std::nullopt);
}

/* ********************************************************************************************** */

ftxui::Element FileInfo::Render() {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::WIDTH;

  const auto& theme = GetTheme().file_info;
  ftxui::Elements lines;

  if (!has_song_info_) {
    // Let user know what to expect from this block (paused song still has its info)
    const std::string hint =
        "Press " + util::EventToString(keybinding::Navigation::Return) + " to play a song";

    lines = {
        ftxui::text("Nothing playing") | ftxui::color(theme.value_empty),
        ftxui::text(hint) | ftxui::color(theme.field),
    };
  } else {
    // Song comes first, then an empty line and its details (audio output is the last one)
    std::vector<Entry> entries{audio_info_};
    entries.insert(entries.end(), output_info_.begin(), output_info_.end());

    lines = {
        ftxui::text(ellipsize(title_, kMaxColumns)) | ftxui::bold | ftxui::color(theme.title),
        ftxui::text(ellipsize(artist_, kMaxColumns)) | ftxui::color(theme.artist),
        ftxui::text(""),
    };

    // Long values are cut with an ellipsis (instead of animated), as the full filename is
    // already animated in files list when selected
    const int width = kMaxColumns - kFieldColumns;

    for (const auto& [field, value] : entries) {
      lines.push_back(ftxui::hbox({
          ftxui::text(field) | ftxui::size(WIDTH, EQUAL, kFieldColumns) | ftxui::color(theme.field),
          ftxui::text(ellipsize(value, width)) | ftxui::color(theme.value),
      }));
    }
  }

  ftxui::Element content = ftxui::vbox(lines);

  return RenderWindow(RenderTitle(" information "), content) |
         ftxui::size(HEIGHT, EQUAL, kMaxRows);
}

/* ********************************************************************************************** */

bool FileInfo::OnEvent(ftxui::Event event) { return event.is_mouse() && OnTitleMouseEvent(event); }

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
  static constexpr std::string_view kUnknownArtist = "Unknown artist";

  title_.clear();
  artist_.clear();
  audio_info_.clear();

  has_song_info_ = !audio.IsEmpty();
  if (!has_song_info_) return;

  // Song is played from a file or streamed from URL
  const bool is_stream = audio.filepath.empty() && audio.stream_info.has_value();
  const std::string source =
      is_stream ? audio.stream_info->base_url : audio.filepath.filename().string();

  title_ = !audio.title.empty() ? audio.title : source;
  artist_ = !audio.artist.empty() ? audio.artist : std::string{kUnknownArtist};

  // Format of audio samples from song (e.g. "44.1 kHz · 16 bits · stereo"), with only what is known
  std::vector<std::string> parts;

  if (audio.sample_rate > 0) parts.push_back(util::format_with_prefix(audio.sample_rate, "Hz"));

  // Bit depth is not applicable for lossy formats (e.g. MP3), as they are not stored as PCM samples
  if (audio.bit_depth > 0) parts.push_back(util::format_with_prefix(audio.bit_depth, "bits"));

  if (audio.num_channels > 0) parts.push_back(ChannelsToString(audio.num_channels));

  std::string format;
  for (const auto& part : parts) {
    if (!format.empty()) format += kSeparator;
    format += part;
  }

  audio_info_ = {
      {is_stream ? "url" : "file", source},
      {"format", !format.empty() ? format : std::string{kUnknown}},
      {"bitrate", audio.bit_rate > 0 ? util::format_with_prefix(audio.bit_rate, "bps")
                                     : std::string{kUnknown}},
      // Same format used by media player
      {"length", model::time_to_string(audio.duration)},
  };
}

/* ********************************************************************************************** */

void FileInfo::ParseAudioOutput(const std::optional<model::AudioOutput>& output) {
  // Format of audio samples sent to output device (e.g. "96 kHz / 32 bits")
  const std::string format =
      output ? util::format_with_prefix(output->format.sample_rate, "Hz") + " / " +
                   util::format_with_prefix(output->format.GetBitDepth(), "bits")
             : std::string{kUnknown};

  output_info_ = {
      {"output", format},
      {"device", output ? output->device : std::string{kUnknown}},
  };
}

}  // namespace interface
