#include "view/block/tab_item/song_lyric.h"

#include <algorithm>
#include <cmath>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <thread>

#include "audio/lyric/lyric_finder.h"
#include "util/formatter.h"
#include "util/logger.h"

namespace interface {

SongLyric::SongLyric(const model::BlockIdentifier& id,
                     const std::shared_ptr<EventDispatcher>& dispatcher, FocusCallback on_focus)
    : TabItem(id, dispatcher, on_focus) {}

/* ********************************************************************************************** */

ftxui::Element SongLyric::Render() {
  ftxui::Element content;
  ftxui::Decorator style = ftxui::color(ftxui::Color::White) | ftxui::bold | ftxui::center;

  if (audio_info_.filepath.empty()) {
    return ftxui::text("No song playing...") | style;
  }

  if (IsFetching()) {
    return ftxui::text("Fetching lyrics...") | style;
  }

  if (IsResultReady()) {
    // It is not that great to have this inside Render(), but it is working fine...
    // TODO: Must rethink this in the near future.
    if (auto result = async_fetcher_.get(); result) lyrics_ = *result;
  }

  if (lyrics_.empty()) {
    return ftxui::text("Failed to fetch =(") | style;
  }

  return DrawSongLyrics(lyrics_);
}

/* ********************************************************************************************** */

bool SongLyric::OnEvent(const ftxui::Event& event) {
  if (lyrics_.empty()) return false;

  int old_focus = focused_;

  // Calculate new index based on upper bound
  if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
    LOG("Handle menu navigation key=", util::EventToString(event));
    focused_ = focused_ - (focused_ > 0 ? 1 : 0);
  }

  if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
    LOG("Handle menu navigation key=", util::EventToString(event));
    focused_ = focused_ + (focused_ < (static_cast<int>(lyrics_.size()) - 1) ? 1 : 0);
  }

  if (event == ftxui::Event::Home) {
    LOG("Handle menu navigation key=", util::EventToString(event));
    focused_ = 0;
  }

  if (event == ftxui::Event::End) {
    LOG("Handle menu navigation key=", util::EventToString(event));
    focused_ = static_cast<int>(lyrics_.size() - 1);
  }

  return focused_ != old_focus ? true : false;
}

/* ********************************************************************************************** */

bool SongLyric::OnCustomEvent(const CustomEvent& event) {
  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    audio_info_ = model::Song{};
    async_fetcher_ = std::future<FetchResult>();
    lyrics_.clear();
    focused_ = 0;
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");
    audio_info_ = event.GetContent<model::Song>();

    if (!audio_info_.filepath.empty()) {
      LOG("Launch async task to fetch song lyrics");
      // As we do not want to hold UI at all, fetch song lyrics asynchronously
      async_fetcher_ = std::async(std::launch::async, std::bind(&SongLyric::FetchSongLyrics, this));
    }
  }

  return false;
}

/* ********************************************************************************************** */

SongLyric::FetchResult SongLyric::FetchSongLyrics() {
  LOG("Started executing thread to fetch song lyrics");

  std::string artist;
  std::string title;

  if (!audio_info_.artist.empty() && !audio_info_.title.empty()) {
    LOG("Getting information from audio metadata");
    artist = audio_info_.artist;
    title = audio_info_.title;
  } else {
    std::string filepath = audio_info_.filepath;
    LOG("Getting information from audio filepath=", filepath);

    size_t pos = filepath.find_last_of('/');
    std::string filename = filepath.substr(pos + 1);

    // If contains more than one hiphen, should not fetch at all
    if (std::string::difference_type n = std::count(filename.begin(), filename.end(), '-'); n > 1) {
      ERROR("Contains more than one hiphen on filename, song lyrics will not be fetched");
      return std::nullopt;
    }

    // Split into artist + title + .extension
    pos = filename.find('-', 0);

    // If filename is not in the expected format ("dummy - song.mp3"), should not fetch song
    if (pos == std::string::npos) {
      ERROR("Filename does not contain a supported pattern");
      return std::nullopt;
    }

    size_t ext_pos = filename.find_last_of('.');

    artist = util::trim(filename.substr(0, pos));
    title = ext_pos != std::string::npos ? util::trim(filename.substr(pos + 1, ext_pos - pos - 1))
                                         : util::trim(filename.substr(pos + 1));
  }

  if (artist.empty() || title.empty()) {
    ERROR("Failed to parse artist and title");
    return std::nullopt;
  }

  FetchResult result = finder_->Search(artist, title);

  if (!result.value().empty()) {
    LOG("Found song lyrics");
  } else {
    ERROR("Failed to fetch song lyrics");
  }

  return result;
}

/* ********************************************************************************************** */

ftxui::Element SongLyric::DrawSongLyrics(const lyric::SongLyric& lyrics) const {
  ftxui::Elements lines;
  bool set_focus = true;
  int count = 0;
  int max_length = 0;

  for (const auto& paragraph : lyrics) {
    std::istringstream input{paragraph};

    for (std::string line; std::getline(input, line);) {
      // Simply add raw line
      lines.push_back(ftxui::text(line));

      // If paragraph index matches the focus index, set focus on element only once
      if (set_focus && count == focused_) {
        lines.back() |= ftxui::focus;
        set_focus = false;
      }

      // Find maximum line length
      if (line.length() > max_length) {
        max_length = (int)line.length();
      }
    }

    // Add a line separator, delimiting the paragraph
    lines.push_back(ftxui::text(""));

    count++;
  }

  // Use maximum length to set width size for text
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  // Format song lyrics in the desired style
  ftxui::Elements formatted_lines;

  for (const auto& line : lines) {
    formatted_lines.push_back(ftxui::hbox({
        ftxui::filler(),
        line | ftxui::size(WIDTH, EQUAL, max_length) | ftxui::color(ftxui::Color::White),
        ftxui::filler(),
    }));
  }

  return ftxui::vbox(formatted_lines) | ftxui::vscroll_indicator | ftxui::frame | ftxui::xflex |
         ftxui::vcenter;
}

}  // namespace interface
