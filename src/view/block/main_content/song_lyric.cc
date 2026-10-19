#include "view/block/main_content/song_lyric.h"

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "audio/lyric/lyric_finder.h"
#include "ftxui/dom/elements.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

namespace {

/**
 * @brief Center song lyrics horizontally (as a block, keeping lines aligned to the left) and make
 * them scrollable
 * @param lines Song lyrics lines (already styled)
 * @param max_length Length from longest line
 * @return UI element
 */
ftxui::Element DrawCentered(const ftxui::Elements& lines, int max_length) {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  ftxui::Elements formatted_lines;

  for (const auto& line : lines) {
    formatted_lines.push_back(ftxui::hbox({
        ftxui::filler(),
        line | ftxui::size(WIDTH, EQUAL, max_length),
        ftxui::filler(),
    }));
  }

  return ftxui::vbox(formatted_lines) | ftxui::vscroll_indicator | ftxui::frame | ftxui::xflex |
         ftxui::vcenter;
}

}  // namespace

/* ********************************************************************************************** */

SongLyric::SongLyric(const model::BlockIdentifier& id,
                     const std::shared_ptr<EventDispatcher>& dispatcher,
                     const FocusCallback& on_focus, const keybinding::Key& keybinding)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string{kTabName}),
      message_{[this] {
                 // Message has expired, so UI must be refreshed to remove it from screen
                 if (auto disp = dispatcher_.lock(); disp) {
                   disp->SendEvent(CustomEvent::Refresh());
                 }
               },
               kMessageDuration},
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
  ftxui::Decorator style = ftxui::color(GetTheme().lyric.text) | ftxui::bold | ftxui::center;

  if (audio_info_.IsEmpty()) {
    return ftxui::text("No song playing...") | style;
  }

  ConsumeSearchResult();

  if (IsFetching()) {
    return ftxui::text("Fetching lyrics...") | style;
  }

  if (!HasLyrics()) {
    return DrawFailure();
  }

  content = synced_.empty() ? DrawSongLyrics(lyrics_) : DrawSyncedLyrics();

  // Draw message (if any) on the upper-right corner, over song lyrics
  if (auto message = message_.GetText(); message.has_value()) {
    content = ftxui::dbox({
        content,
        ftxui::vbox({
            ftxui::hbox({
                ftxui::filler(),
                ftxui::text(*message) | ftxui::bold | ftxui::color(GetTheme().lyric.text),
            }),
            ftxui::filler(),
        }),
    });
  }

  return content | ftxui::reflect(box_);
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

  // Go back to follow line being sung, after user scrolled synced song lyrics
  if (event == keybinding::Lyric::Follow && !synced_.empty()) {
    LOG("Handle key to follow current line from synced song lyrics");
    follow_ = true;
    return true;
  }

  // Shift synced song lyrics, when they are not in time with song
  if ((event == keybinding::Lyric::OffsetEarlier || event == keybinding::Lyric::OffsetLater) &&
      !synced_.empty()) {
    LOG("Handle key to change offset from synced song lyrics");
    ChangeOffset(event == keybinding::Lyric::OffsetEarlier ? kOffsetStep : -kOffsetStep);
    return true;
  }

  if (!HasLyrics()) return false;

  const bool is_navigation = event == Keybind::ArrowUp || event == Keybind::Up ||
                             event == Keybind::ArrowDown || event == Keybind::Down ||
                             event == Keybind::Home || event == Keybind::End;

  if (!is_navigation) return false;

  LOG("Handle menu navigation key=", std::quoted(util::EventToString(event)));

  // Synced song lyrics scroll by line, while plain song lyrics scroll by paragraph
  const int last = static_cast<int>(synced_.empty() ? lyrics_.size() : synced_.size()) - 1;

  // Manual scroll on synced song lyrics starts from line being sung, and stops following it
  const bool stop_following = !synced_.empty() && follow_;
  if (stop_following) {
    focused_ = std::max(current_, 0);
    follow_ = false;
  }

  int old_focus = focused_;

  // Calculate new index based on upper bound
  if (event == Keybind::ArrowUp || event == Keybind::Up) {
    focused_ = focused_ - (focused_ > 0 ? 1 : 0);
  }

  if (event == Keybind::ArrowDown || event == Keybind::Down) {
    focused_ = focused_ + (focused_ < last ? 1 : 0);
  }

  if (event == Keybind::Home) {
    focused_ = 0;
  }

  if (event == Keybind::End) {
    focused_ = last;
  }

  return focused_ != old_focus || stop_following;
}

/* ********************************************************************************************** */

bool SongLyric::OnMouseEvent(ftxui::Event& event) {
  const auto& mouse = event.mouse();
  const bool wheel_up = mouse.button == ftxui::Mouse::WheelUp;

  if (!wheel_up && mouse.button != ftxui::Mouse::WheelDown) return false;

  if (!HasLyrics() || !box_.Contain(mouse.x, mouse.y)) return false;

  // Set focus on parent block, so keys go to song lyric after scrolling it
  if (on_focus_) on_focus_();

  // Reuse handler from keyboard, to scroll exactly like its keys
  OnEvent(wheel_up ? keybinding::Navigation::ArrowUp : keybinding::Navigation::ArrowDown);
  return true;
}

/* ********************************************************************************************** */

bool SongLyric::OnCustomEvent(const CustomEvent& event) {
  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    audio_info_ = model::Song{};
    CancelFetching();
    lyrics_.clear();
    synced_.clear();
    artist_.clear();
    title_.clear();
    status_.reset();
    focused_ = 0;
    position_ms_ = 0;
    offset_ms_ = 0;
    current_ = -1;
    follow_ = true;
    message_.Hide();
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");
    audio_info_ = event.GetContent<model::Song>();
    position_ms_ = 0;
    ParseSearchTerms();

    if (!audio_info_.IsEmpty()) {
      StartFetching();
    }
  }

  // Do not return true because other blocks may use it
  if (event == CustomEvent::Identifier::UpdateSongState) {
    const uint32_t position = event.GetContent<model::Song::CurrentInformation>().position_ms;

    // Position jumping by more than a second means a seek, so follow line being sung again
    if (position > position_ms_ + kSeekThreshold || position + kSeekThreshold < position_ms_) {
      follow_ = true;
    }

    position_ms_ = position;
    UpdateCurrentLine();
  }

  return false;
}

/* ********************************************************************************************** */

void SongLyric::StartFetching() {
  // Reset any result from last search
  CancelFetching();
  lyrics_.clear();
  synced_.clear();
  status_.reset();
  focused_ = 0;
  offset_ms_ = 0;
  current_ = -1;
  follow_ = true;

  if (!HasSearchTerms()) {
    INFO("Missing artist or title, song lyrics will not be fetched");
    return;
  }

  LOG("Send request to fetch song lyrics");

  {
    std::scoped_lock lock(mutex_);
    pending_ = Request{
        .id = ++request_id_, .artist = artist_, .title = title_, .duration = audio_info_.duration};
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
  synced_ = std::move(result_->synced);
  result_.reset();
  fetching_ = false;

  UpdateCurrentLine();
}

/* ********************************************************************************************** */

void SongLyric::UpdateCurrentLine() {
  // Line starts when song reaches its timestamp (sooner when offset is positive)
  const int64_t position = static_cast<int64_t>(position_ms_) + offset_ms_;

  auto next = std::upper_bound(
      synced_.begin(), synced_.end(), position,
      [](int64_t pos, const model::LyricLine& line) { return pos < line.timestamp; });

  current_ = static_cast<int>(std::distance(synced_.begin(), next)) - 1;
}

/* ********************************************************************************************** */

void SongLyric::ChangeOffset(int32_t step) {
  offset_ms_ = std::clamp(offset_ms_ + step, -kMaxOffset, kMaxOffset);
  UpdateCurrentLine();

  // Show offset in seconds, e.g. "+0.3s" (positive means lyrics are shown earlier)
  std::ostringstream text;
  text << "lyrics offset: " << std::showpos << std::fixed << std::setprecision(1)
       << offset_ms_ / 1000.0 << "s";

  message_.Show(text.str());
}

/* ********************************************************************************************** */

void SongLyric::FetchLoop() {
  util::Logger::SetThreadName("lyrics");
  std::unique_lock lock(mutex_);

  while (true) {
    notifier_.wait(lock, [this] { return exit_ || pending_.has_value(); });
    if (exit_) return;

    Request request = std::move(*pending_);
    pending_.reset();

    // Search without holding the lock, so UI thread never waits for it
    lock.unlock();

    finder_->SetCancelCheck([this, id = request.id] { return exit_ || id != request_id_; });
    lyric::SearchResult result = finder_->Search(request.artist, request.title, request.duration);

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
    LOG("Streamed song title does not contain artist and title");
    return;
  }

  const std::string filepath = audio_info_.filepath;
  LOG("Getting information from audio filepath=", filepath);

  size_t pos = filepath.find_last_of('/');
  std::string filename = filepath.substr(pos + 1);

  // If contains more than one hiphen, should not fetch at all
  if (const std::string::difference_type n = std::count(filename.begin(), filename.end(), '-');
      n > 1) {
    LOG("Contains more than one hiphen on filename, song lyrics will not be fetched");
    return;
  }

  // Split into artist + title + .extension
  pos = filename.find('-', 0);

  // If filename is not in the expected format ("dummy - song.mp3"), should not fetch song
  if (pos == std::string::npos) {
    LOG("Filename does not contain a supported pattern");
    return;
  }

  const size_t ext_pos = filename.find_last_of('.');

  artist_ = util::trim(filename.substr(0, pos));
  title_ = ext_pos != std::string::npos ? util::trim(filename.substr(pos + 1, ext_pos - pos - 1))
                                        : util::trim(filename.substr(pos + 1));

  // Both must be filled, otherwise search is not possible
  if (!HasSearchTerms()) {
    LOG("Failed to parse artist and title");
    artist_.clear();
    title_.clear();
  }
}

/* ********************************************************************************************** */

ftxui::Element SongLyric::DrawFailure() const {
  auto line = [](const std::string& content) {
    return ftxui::text(content) | ftxui::color(GetTheme().lyric.text) | ftxui::hcenter;
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
          line("Could not reach lyrics websites (network error or server busy)") | ftxui::bold,
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
      lines.push_back(ftxui::text(line) | ftxui::color(GetTheme().lyric.text));

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

  return DrawCentered(lines, max_length);
}

/* ********************************************************************************************** */

ftxui::Element SongLyric::DrawSyncedLyrics() const {
  const auto& theme = GetTheme().lyric;
  const int focus = follow_ ? std::max(current_, 0) : focused_;

  ftxui::Elements lines;
  int max_length = 0;

  for (int index = 0; index < static_cast<int>(synced_.size()); index++) {
    const std::string& text = synced_[index].text;

    // Line being sung is highlighted, while the others are dimmed
    ftxui::Element line = ftxui::text(text);
    line |= index == current_ ? ftxui::color(theme.highlight) | ftxui::bold
                              : ftxui::color(theme.text) | ftxui::dim;

    if (index == focus) line |= ftxui::focus;

    lines.push_back(line);
    max_length = std::max(max_length, static_cast<int>(text.length()));
  }

  return DrawCentered(lines, max_length);
}

}  // namespace interface
