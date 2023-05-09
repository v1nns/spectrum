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
                     const std::shared_ptr<EventDispatcher>& dispatcher,
                     driver::UrlFetcher* fetcher, driver::HtmlParser* parser)
    : TabItem(id, dispatcher), finder_{lyric::LyricFinder::Create(fetcher, parser)} {}

/* ********************************************************************************************** */

ftxui::Element SongLyric::Render() {
  ftxui::Element content;

  if (audio_info_.filepath.empty()) {
    return ftxui::text("No song playing...") | ftxui::bold | ftxui::center;
  }

  if (IsFetching()) {
    return ftxui::text("Fetching lyrics...") | ftxui::bold | ftxui::center;
  }

  if (IsResultReady()) {
    // It is not that great to have this inside Render(), but it is working fine...
    // TODO: Must rethink this in the near future.
    if (auto result = async_fetcher_.get(); result) lyrics_ = *result;
  }

  if (lyrics_.empty()) {
    return ftxui::text("Failed to fetch =(") | ftxui::bold | ftxui::center;
  }

  ftxui::Elements lines;

  for (const auto& paragraph : lyrics_) {
    std::istringstream input{paragraph};

    for (std::string line; std::getline(input, line);) {
      lines.push_back(ftxui::text(line));
    }
  }

  return ftxui::vbox(lines) | ftxui::frame | ftxui::center;
}

/* ********************************************************************************************** */

bool SongLyric::OnEvent(const ftxui::Event& event) { return false; }

/* ********************************************************************************************** */

bool SongLyric::OnCustomEvent(const CustomEvent& event) {
  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    audio_info_ = model::Song{};
    async_fetcher_ = std::future<FetchResult>();
    lyrics_.clear();
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

}  // namespace interface
