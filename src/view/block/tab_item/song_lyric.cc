#include "view/block/tab_item/song_lyric.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <thread>

#include "util/formatter.h"
#include "util/logger.h"

namespace interface {

static void DrawEllipse(ftxui::Canvas& content, int x1, int y1, int r1, int r2, bool wtf) {
  int x = -r1;
  int y = 0;
  int e2 = r2;
  int dx = (1 + 2 * x) * e2 * e2;
  int dy = x * x;
  int err = dx + dy;

  do {
    if (wtf) {
      content.DrawPoint(x1 + x, y1 + y, true);
      content.DrawPoint(x1 + x, y1 - y, true);
    } else {
      content.DrawPoint(x1 - x, y1 + y, true);
      content.DrawPoint(x1 - x, y1 - y, true);
    }

    e2 = 2 * err;
    if (e2 >= dx) {
      x++;
      err += dx += 2 * r2 * r2;
    }
    if (e2 <= dy) {
      y++;
      err += dy += 2 * r1 * r1;
    }
  } while (x <= 0);

  while (y++ < r2) {
    content.DrawPoint(x1, y1 + y, true);
    content.DrawPoint(x1, y1 - y, true);
  }
}

static ftxui::Element RenderNotFound() {
  ftxui::Canvas content(100, 200);

  // O
  content.DrawPointCircle(30, 40, 10);
  content.DrawPointCircle(30, 40, 11);
  content.DrawPointCircle(30, 40, 12);

  // o
  content.DrawPointCircle(53, 42, 8);
  content.DrawPointCircle(53, 42, 9);

  // p
  content.DrawPointLine(64, 34, 64, 55);
  content.DrawPointLine(65, 34, 65, 55);

  content.DrawPointEllipse(71, 43, 6, 6);
  content.DrawPointEllipse(71, 43, 7, 7);

  // s
  DrawEllipse(content, 88, 39, 8, 4, true);
  DrawEllipse(content, 89, 40, 8, 4, true);
  DrawEllipse(content, 85, 46, 8, 4, false);
  DrawEllipse(content, 86, 47, 8, 4, false);

  return ftxui::canvas(content) | ftxui::center;
}

/* ********************************************************************************************** */

SongLyric::SongLyric(const model::BlockIdentifier& id,
                     const std::shared_ptr<EventDispatcher>& dispatcher)
    : TabItem(id, dispatcher) {}

/* ********************************************************************************************** */

ftxui::Element SongLyric::Render() {
  ftxui::Element content;

  if (audio_info_.filepath.empty()) {
    return ftxui::text("No song playing...") | ftxui::bold | ftxui::center;
  }

  if (fetching_)
    content = ftxui::text("Fetching lyrics...") | ftxui::bold;
  else if (failed_)
    content = ftxui::text("Failed to fetch =(") | ftxui::bold;
  else if (lyrics_) {
    ftxui::Elements lines;

    for (auto& paragraph : *lyrics_) {
      std::istringstream input{paragraph};

      for (std::string line; std::getline(input, line);) {
        lines.push_back(ftxui::text(line));
      }
    }

    content = ftxui::vbox(lines) | ftxui::frame;
  }

  return content | ftxui::center;
}

/* ********************************************************************************************** */

bool SongLyric::OnEvent(const ftxui::Event& event) { return false; }

/* ********************************************************************************************** */

bool SongLyric::OnCustomEvent(const CustomEvent& event) {
  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    audio_info_ = model::Song{};
    lyrics_.reset();
    failed_ = false;
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");
    audio_info_ = event.GetContent<model::Song>();

    if (!audio_info_.filepath.empty()) {
      LOG("Launch thread to fetch song lyrics");
      // As we do not want to hold UI at all, fetch song lyrics in a thread
      std::thread(&SongLyric::FetchSong, this).detach();
    }
  }

  return false;
}

/* ********************************************************************************************** */

void SongLyric::FetchSong() {
  std::scoped_lock lock(mutex_);
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
      return;
    }

    // Split into artist + title + .extension
    pos = filename.find('-', 0);

    // If filename is not in the expected format ("dummy - song.mp3"), should not fetch song
    if (pos == std::string::npos) {
      ERROR("Filename does not contain a supported pattern");
      return;
    }

    size_t ext_pos = filename.find_last_of('.');

    artist = util::trim(filename.substr(0, pos));
    title = ext_pos != std::string::npos ? util::trim(filename.substr(pos + 1, ext_pos - pos - 1))
                                         : util::trim(filename.substr(pos + 1));
  }

  if (artist.empty() || title.empty()) {
    ERROR("Failed to parse artist and title");
    failed_ = true;
    return;
  }

  fetching_ = true;
  auto result = finder_->Search(artist, title);
  fetching_ = false;

  if (!result.empty()) {
    LOG("Found song lyrics");
    lyrics_ = std::move(result);
  } else {
    ERROR("Failed to fetch song lyrics");
    failed_ = true;
  }
}

}  // namespace interface