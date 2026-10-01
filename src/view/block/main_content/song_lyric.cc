#include "view/block/main_content/song_lyric.h"

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <iomanip>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "audio/lyric/lyric_finder.h"
#include "ftxui/dom/elements.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {

SongLyric::SongLyric(const model::BlockIdentifier& id,
                     const std::shared_ptr<EventDispatcher>& dispatcher,
                     const FocusCallback& on_focus, const keybinding::Key& keybinding)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string{kTabName}),
      fetcher_{&SongLyric::FetchLoop, this} {}

/* ********************************************************************************************** */

SongLyric::~SongLyric() {
  {
    // Any search in progress is canceled as soon as exit flag is set
    std::scoped_lock lock(mutex_);
    exit_ = true;
  }

  notifier_.notify_one();
  fetcher_.join();
}

/* ********************************************************************************************** */

ftxui::Element SongLyric::Render() {
  ftxui::Element content;
  ftxui::Decorator style = ftxui::color(ftxui::Color::White) | ftxui::bold | ftxui::center;

  if (audio_info_.IsEmpty()) {
    return ftxui::text("No song playing...") | style;
  }

  ConsumeSearchResult();

  if (IsFetching()) {
    return ftxui::text("Fetching lyrics...") | style;
  }

  if (lyrics_.empty()) {
    return DrawFailure();
  }

  return DrawSongLyrics(lyrics_);
}

/* ********************************************************************************************** */

bool SongLyric::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  // Search again for song lyrics, as last attempt did not find them
  if (event == keybinding::Lyric::Retry && CanRetry()) {
    LOG("Handle key to retry fetching song lyrics");
    StartFetching();
    return true;
  }

  if (lyrics_.empty()) return false;

  int old_focus = focused_;

  // Calculate new index based on upper bound
  if (event == Keybind::ArrowUp || event == Keybind::Up) {
    LOG("Handle menu navigation key=", std::quoted(util::EventToString(event)));
    focused_ = focused_ - (focused_ > 0 ? 1 : 0);
  }

  if (event == Keybind::ArrowDown || event == Keybind::Down) {
    LOG("Handle menu navigation key=", std::quoted(util::EventToString(event)));
    focused_ = focused_ + (focused_ < (static_cast<int>(lyrics_.size()) - 1) ? 1 : 0);
  }

  if (event == Keybind::Home) {
    LOG("Handle menu navigation key=", std::quoted(util::EventToString(event)));
    focused_ = 0;
  }

  if (event == Keybind::End) {
    LOG("Handle menu navigation key=", std::quoted(util::EventToString(event)));
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
    CancelFetching();
    lyrics_.clear();
    artist_.clear();
    title_.clear();
    status_.reset();
    focused_ = 0;
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");
    audio_info_ = event.GetContent<model::Song>();
    ParseSearchTerms();

    if (!audio_info_.IsEmpty()) {
      StartFetching();
    }
  }

  return false;
}

/* ********************************************************************************************** */

void SongLyric::StartFetching() {
  // Reset any result from last search
  CancelFetching();
  lyrics_.clear();
  status_.reset();
  focused_ = 0;

  if (!HasSearchTerms()) {
    ERROR("Missing artist or title, song lyrics will not be fetched");
    return;
  }

  LOG("Send request to fetch song lyrics");

  {
    std::scoped_lock lock(mutex_);
    pending_ = Request{.id = ++request_id_, .artist = artist_, .title = title_};
    fetching_ = true;
  }

  notifier_.notify_one();
}

/* ********************************************************************************************** */

void SongLyric::CancelFetching() {
  std::scoped_lock lock(mutex_);

  // Changing request identifier cancels search in progress and discards its result
  request_id_++;
  pending_.reset();
  result_.reset();
  fetching_ = false;
}

/* ********************************************************************************************** */

void SongLyric::ConsumeSearchResult() {
  std::scoped_lock lock(mutex_);
  if (!result_.has_value()) return;

  status_ = result_->status;
  lyrics_ = std::move(result_->lyrics);
  result_.reset();
  fetching_ = false;
}

/* ********************************************************************************************** */

void SongLyric::FetchLoop() {
  std::unique_lock lock(mutex_);

  while (true) {
    notifier_.wait(lock, [this] { return exit_ || pending_.has_value(); });
    if (exit_) return;

    Request request = std::move(*pending_);
    pending_.reset();

    // Search without holding the lock, so UI thread never waits for it
    lock.unlock();

    finder_->SetCancelCheck([this, id = request.id] { return exit_ || id != request_id_; });
    lyric::SearchResult result = finder_->Search(request.artist, request.title);

    lock.lock();

    // Discard result from canceled request
    if (exit_ || request.id != request_id_) continue;

    result_ = std::move(result);

    // Ask for a UI refresh, so search result is rendered right away
    if (auto dispatcher = dispatcher_.lock(); dispatcher) {
      lock.unlock();
      dispatcher->SendEvent(CustomEvent::Refresh());
      lock.lock();
    }
  }
}

/* ********************************************************************************************** */

bool SongLyric::CanRetry() {
  // Retrying makes sense only when search was executed and did not find anything
  const bool search_failed = status_ == lyric::SearchResult::Status::NotFound ||
                             status_ == lyric::SearchResult::Status::FetchFailed;

  return search_failed && !IsFetching();
}

/* ********************************************************************************************** */

void SongLyric::ParseSearchTerms() {
  artist_.clear();
  title_.clear();

  if (!audio_info_.artist.empty() && !audio_info_.title.empty()) {
    LOG("Getting information from audio metadata");
    artist_ = audio_info_.artist;
    title_ = audio_info_.title;
    return;
  }

  // Streamed song does not have a filename, its information comes only from its title
  if (audio_info_.stream_info.has_value()) {
    ERROR("Streamed song title does not contain artist and title");
    return;
  }

  const std::string filepath = audio_info_.filepath;
  LOG("Getting information from audio filepath=", filepath);

  size_t pos = filepath.find_last_of('/');
  std::string filename = filepath.substr(pos + 1);

  // If contains more than one hiphen, should not fetch at all
  if (const std::string::difference_type n = std::count(filename.begin(), filename.end(), '-');
      n > 1) {
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

  const size_t ext_pos = filename.find_last_of('.');

  artist_ = util::trim(filename.substr(0, pos));
  title_ = ext_pos != std::string::npos ? util::trim(filename.substr(pos + 1, ext_pos - pos - 1))
                                        : util::trim(filename.substr(pos + 1));

  // Both must be filled, otherwise search is not possible
  if (!HasSearchTerms()) {
    ERROR("Failed to parse artist and title");
    artist_.clear();
    title_.clear();
  }
}

/* ********************************************************************************************** */

ftxui::Element SongLyric::DrawFailure() const {
  auto line = [](const std::string& content) {
    return ftxui::text(content) | ftxui::color(ftxui::Color::White) | ftxui::hcenter;
  };

  const std::string retry_hint = util::EventToString(keybinding::Lyric::Retry) + ": retry search";

  ftxui::Elements lines;

  if (!HasSearchTerms()) {
    lines = {
        line("Cannot search lyrics without artist and title") | ftxui::bold,
        line(audio_info_.stream_info.has_value()
                 ? "(video title is not in the \"Artist - Title\" format)"
                 : "(add them to metadata or name the file as \"Artist - Title\")") |
            ftxui::dim,
    };

    return ftxui::vbox(lines) | ftxui::center;
  }

  switch (status_.value_or(lyric::SearchResult::Status::NotFound)) {
    case lyric::SearchResult::Status::NotFound:
      lines = {
          line("Lyrics not found for") | ftxui::bold,
          line("\"" + artist_ + " - " + title_ + "\"") | ftxui::bold,
          ftxui::text(""),
          line(retry_hint) | ftxui::dim,
      };
      break;

    case lyric::SearchResult::Status::FetchFailed:
      lines = {
          line("Could not reach lyrics websites (network error)") | ftxui::bold,
          ftxui::text(""),
          line(retry_hint) | ftxui::dim,
      };
      break;

    case lyric::SearchResult::Status::Found:
      // Should not happen, as lyrics are drawn instead of this
      lines = {line("Lyrics not available") | ftxui::bold};
      break;
  }

  return ftxui::vbox(lines) | ftxui::center;
}

/* ********************************************************************************************** */

ftxui::Element SongLyric::DrawSongLyrics(const model::SongLyric& lyrics) const {
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
