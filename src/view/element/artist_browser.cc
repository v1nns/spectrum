#include "view/element/artist_browser.h"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <utility>

#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/custom_event.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"
#include "view/element/util.h"

namespace interface {

namespace {

//! Maximum columns for who created playlist (shown on the right, so title gets most of the space)
constexpr int kMaxUploaderColumns = 12;

//! Format number of followers as shown by YouTube (e.g. "5.5M", "37K" or "126")
std::string FormatFollowers(uint64_t followers) {
  static constexpr double kThousand = 1e3;
  static constexpr double kMillion = 1e6;

  //! Values below this one are shown with a decimal place (e.g. "5.5M")
  static constexpr double kMaxWithDecimal = 10;

  auto format = [](double value, const char* suffix) {
    std::ostringstream ss;

    if (value < kMaxWithDecimal) {
      ss << std::fixed << std::setprecision(1) << value;
    } else {
      ss << static_cast<uint64_t>(value);
    }

    std::string text = ss.str();
    if (text.size() > 2 && text.compare(text.size() - 2, 2, ".0") == 0)
      text.resize(text.size() - 2);

    return text + suffix;
  };

  const auto value = static_cast<double>(followers);

  if (value >= kMillion) return format(value / kMillion, "M");
  if (value >= kThousand) return format(value / kThousand, "K");

  return std::to_string(followers);
}

//! Format song duration as minutes and seconds (e.g. "4:13")
std::string FormatDuration(uint32_t duration) {
  static constexpr uint32_t kSecondsPerMinute = 60;
  if (duration == 0) return "";

  std::ostringstream ss;
  ss << duration / kSecondsPerMinute << ":" << std::setw(2) << std::setfill('0')
     << duration % kSecondsPerMinute;

  return ss.str();
}

//! Get reason why something could not be fetched (in a few words, as there is not much space)
std::string GetFailureReason(error::Code code, const std::string& fallback) {
  switch (code) {
    case error::kStreamFetcherNotFound:
      return "yt-dlp not found";
    case error::kStreamBlocked:
    case error::kStreamBlockedWithCookies:
      return "Refused by YouTube";
    case error::kStreamCookiesFailed:
      return "Cannot read cookies";
    case error::kStreamTimedOut:
      return "Took too long to answer";
    default:
      return fallback;
  }
}

//! Get URL from YouTube playlist (releases are also playlists)
std::string GetPlaylistUrl(const std::string& id) {
  return "https://www.youtube.com/playlist?list=" + id;
}

}  // namespace

/* ********************************************************************************************** */

ArtistBrowser::ArtistBrowser(const std::shared_ptr<EventDispatcher>& dispatcher,
                             const FetchCallbacks& fetch, const PlaylistCallbacks& playlist)
    : dispatcher_{dispatcher},
      fetch_{fetch},
      playlist_{playlist},
      menu_{menu::CreateYoutubeMenu(
          dispatcher, [this] { Refresh(); },

          // Callback triggered on menu item click (or Return key)
          [this](const std::optional<Row>& active) {
            if (!active) return false;
            if (Open(*active)) return true;

            Toggle(*active);
            return true;
          },

          // Callback to get state from entry, which may change at any time
          [this](const Row& row) { return GetRowState(row); })},
      job_{[this] { Refresh(); }, "search"} {}

/* ********************************************************************************************** */

ftxui::Element ArtistBrowser::Render() {
  using Keybind = keybinding::Navigation;
  const auto& theme = GetTheme().dialog;

  ftxui::Elements content{
      ftxui::text(""),
      ellipsized_text(GetHeader(), 1) | ftxui::color(theme.label),
      ftxui::text(""),
      menu_->Render() | ftxui::flex,
  };

  // What is being fetched, or why it could not be fetched
  if (status_.has_value()) {
    content.push_back(ftxui::text(std::string{spinner_frame(job_.GetElapsed())} + " " + *status_) |
                      ftxui::color(theme.hint));
  } else if (error_.has_value()) {
    content.push_back(ellipsized_text("✗ " + *error_, 1) | ftxui::color(theme.error) | ftxui::bold);
  } else {
    content.push_back(ftxui::text(""));
  }

  // Hints are only shown when there is enough space for them (they are also in help dialog)
  if (max_lines_ >= kMinLinesForHints) {
    const std::string open = util::EventToString(Keybind::Return) + ": open · " +
                             util::EventToString(Keybind::Escape) + ": back";

    ftxui::Elements hints;
    if (level_ == Level::Releases) hints.push_back(ftxui::text("Space: add/remove"));
    hints.push_back(ftxui::text(open));

    content.push_back(ftxui::text(""));
    content.push_back(ftxui::vbox(std::move(hints)) | ftxui::color(theme.hint) | ftxui::dim);
  }

  auto padding = ftxui::text(std::string(kPadding, ' '));

  return ftxui::hbox({padding, ftxui::vbox(std::move(content)) | ftxui::xflex, padding}) |
         ftxui::reflect(Box());
}

/* ********************************************************************************************** */

bool ArtistBrowser::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (event == Keybind::Escape || event == Keybind::Backspace) {
    Back();
    return true;
  }

  // Menu handles navigation (and Return key, to open entry)
  if (menu_->OnEvent(event)) return true;

  auto active = menu_->GetActiveEntry();
  if (!active.has_value()) return false;

  if (event == Keybind::Space) {
    Toggle(*active);
    return true;
  }

  // Only open release (as Return key would also close it, when it is already open)
  if (event == Keybind::Right || event == Keybind::ArrowRight) {
    bool expanded = active->kind == Row::Kind::Release && releases_.at(active->index).expanded;
    if (!expanded && active->kind != Row::Kind::Track) Open(*active);
    return true;
  }

  // Close release (from any of its tracks too)
  if (event == Keybind::Left || event == Keybind::ArrowLeft) {
    if (level_ == Level::Releases) {
      releases_.at(active->index).expanded = false;
      UpdateRows();
    }
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool ArtistBrowser::OnMouseEvent(ftxui::Event& event) { return menu_->OnMouseEvent(event); }

/* ********************************************************************************************** */

void ArtistBrowser::Search(const std::string& query) {
  Reset();
  query_ = query;

  LOG("Search channels from artist=", std::quoted(query));

  StartJob("Searching artists",
           [this, search = fetch_.search_artists,
            query](const std::atomic<bool>* cancel) -> BackgroundJob::Result {
             std::vector<model::YoutubeChannel> channels;
             error::Code code =
                 search ? search(query, channels, cancel) : error::kStreamFetchFailed;

             return [this, code, channels = std::move(channels)] {
               status_.reset();

               if (code != error::kSuccess) {
                 error_ = GetFailureReason(code, "Cannot search artists");
               } else if (channels.empty()) {
                 error_ = "No artists found";
               }

               channels_ = channels;
               UpdateRows();
               menu_->ResetState();
             };
           });

  UpdateRows();
}

/* ********************************************************************************************** */

void ArtistBrowser::Reset() {
  job_.Stop();

  level_ = Level::Channels;
  query_.clear();
  channels_.clear();
  channel_.reset();
  releases_.clear();

  status_.reset();
  loading_.reset();
  error_.reset();

  channel_selected_ = 0;
  UpdateRows();
}

/* ********************************************************************************************** */

void ArtistBrowser::SetMaxColumns(int max_columns) {
  menu_->SetMaxColumns(max_columns - 2 * kPadding);
}

/* ********************************************************************************************** */

bool ArtistBrowser::Open(const Row& row) {
  using Kind = Row::Kind;

  // Only one thing is fetched at a time
  if (job_.IsRunning() && row.kind != Kind::Track) return true;

  switch (row.kind) {
    case Kind::Channel:
    case Kind::Fallback: {
      const bool fallback = row.kind == Kind::Fallback;
      channel_selected_ = fallback ? static_cast<int>(channels_.size()) : row.index;

      std::optional<int> channel = fallback ? std::nullopt : std::optional{row.index};
      const std::string source = fallback ? query_ : channels_.at(row.index).id;
      auto fetch = fallback ? fetch_.search_playlists : fetch_.fetch_releases;

      LOG("Open releases from=", std::quoted(source), " fallback=", fallback);

      StartJob(
          fallback ? "Searching playlists" : "Loading albums",
          [this, fetch, source, channel](const std::atomic<bool>* cancel) -> BackgroundJob::Result {
            std::vector<model::YoutubeRelease> releases;
            error::Code code = fetch ? fetch(source, releases, cancel) : error::kStreamFetchFailed;

            return [this, code, channel, releases = std::move(releases)] {
              status_.reset();

              if (code != error::kSuccess) {
                error_ = GetFailureReason(
                    code, channel ? "Cannot load albums" : "Cannot search playlists");
                return;
              }

              // Stay on channels, so user can pick another one
              if (releases.empty()) {
                error_ = channel ? "No albums on this channel" : "No playlists found";
                return;
              }

              level_ = Level::Releases;
              channel_ = channel;
              releases_.clear();

              for (const auto& release : releases) releases_.push_back(Release{.info = release});

              UpdateRows();
              menu_->ResetState();
            };
          });
      return true;
    }

    case Kind::Release: {
      auto& release = releases_.at(row.index);

      if (!release.tracks.has_value()) {
        LoadTracks(row.index, false);
        return true;
      }

      release.expanded = !release.expanded;
      UpdateRows();
      return true;
    }

    case Kind::Track:
      break;
  }

  return false;
}

/* ********************************************************************************************** */

void ArtistBrowser::Toggle(const Row& row) {
  if (row.kind != Row::Kind::Release && row.kind != Row::Kind::Track) return;

  const auto& release = releases_.at(row.index);

  // Tracks are fetched first, then all of them are added
  if (!release.tracks.has_value()) {
    LoadTracks(row.index, true);
    return;
  }

  std::vector<model::Song> songs = *release.tracks;
  if (row.kind == Row::Kind::Track) songs = {release.tracks->at(row.track)};

  const bool all = std::all_of(songs.begin(), songs.end(), playlist_.contains);

  LOG("Toggle songs in playlist, songs=", songs.size(), " remove=", all);
  all ? playlist_.remove(songs) : playlist_.add(songs);
}

/* ********************************************************************************************** */

void ArtistBrowser::Back() {
  // Stop what is being fetched, staying where it was started from
  if (job_.IsRunning()) {
    LOG("Stop fetching from artist browser");
    job_.Stop();

    status_.reset();
    loading_.reset();

    // Search itself was stopped, so there is nothing to show
    if (level_ == Level::Channels && channels_.empty()) playlist_.back();

    UpdateRows();
    return;
  }

  error_.reset();

  if (level_ == Level::Channels) {
    playlist_.back();
    return;
  }

  level_ = Level::Channels;
  channel_.reset();
  releases_.clear();

  UpdateRows();
  menu_->ResetState(channel_selected_);
}

/* ********************************************************************************************** */

void ArtistBrowser::LoadTracks(int index, bool add) {
  // Only one thing is fetched at a time
  if (job_.IsRunning()) return;

  const std::string url = GetPlaylistUrl(releases_.at(index).info.id);
  LOG("Load tracks from release=", std::quoted(url), " add=", add);

  loading_ = index;

  BackgroundJob::Task task = [this, fetch = fetch_.fetch_tracks, url, index,
                              add](const std::atomic<bool>* cancel) -> BackgroundJob::Result {
    std::vector<model::Song> songs;
    error::Code code = fetch ? fetch(url, songs, cancel) : error::kStreamFetchFailed;

    return [this, code, index, add, songs = std::move(songs)] {
      loading_.reset();

      if (code != error::kSuccess || songs.empty()) {
        error_ = code != error::kSuccess ? GetFailureReason(code, "Cannot load album")
                                         : "No songs in this album";
        return;
      }

      auto& release = releases_.at(index);
      release.tracks = songs;

      if (add) {
        playlist_.add(songs);
      } else {
        release.expanded = true;
      }

      UpdateRows();
    };
  };

  error_.reset();
  status_.reset();
  job_.Start(task);
}

/* ********************************************************************************************** */

void ArtistBrowser::StartJob(const std::string& status, const BackgroundJob::Task& task) {
  status_ = status;
  error_.reset();
  job_.Start(task);
}

/* ********************************************************************************************** */

void ArtistBrowser::UpdateRows() {
  using Kind = Row::Kind;

  auto active = menu_->GetActiveEntry();
  std::vector<Row> rows;

  if (level_ == Level::Channels) {
    for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
      const auto& channel = channels_.at(i);
      rows.push_back(Row{
          .kind = Kind::Channel,
          .index = i,
          .text = channel.name + (channel.verified ? " ✓" : ""),
          .detail = channel.followers ? FormatFollowers(*channel.followers) : "",
      });
    }

    // Playlists may also be searched after channels were searched
    if (!query_.empty() && !status_.has_value()) {
      rows.push_back(Row{.kind = Kind::Fallback,
                         .index = static_cast<int>(channels_.size()),
                         .text = "Search playlists for \"" + query_ + "\""});
    }
  } else {
    for (int i = 0; i < static_cast<int>(releases_.size()); ++i) {
      const auto& release = releases_.at(i);

      // Playlist created by anyone tells who did it, as it may not be from the artist
      std::string detail =
          channel_.has_value() ? "" : ellipsize(release.info.uploader, kMaxUploaderColumns);
      if (channel_.has_value() && release.tracks.has_value()) {
        detail = std::to_string(release.tracks->size());
      }

      rows.push_back(
          Row{.kind = Kind::Release, .index = i, .text = release.info.title, .detail = detail});

      if (!release.expanded || !release.tracks.has_value()) continue;

      for (int t = 0; t < static_cast<int>(release.tracks->size()); ++t) {
        const auto& song = release.tracks->at(t);

        // Tracks from a release are all from the same artist (unlike any other playlist)
        const std::string title =
            channel_.has_value() && !song.title.empty() ? song.title : song.GetTitle();

        rows.push_back(Row{.kind = Kind::Track,
                           .index = i,
                           .track = t,
                           .text = title,
                           .detail = FormatDuration(song.duration)});
      }
    }
  }

  menu_->SetEntries(rows);

  if (!active.has_value()) return;

  // Keep the same entry selected (or its release, when tracks are not shown anymore)
  auto it = std::find_if(rows.begin(), rows.end(), [&active](const Row& row) {
    return row.kind == active->kind && row.index == active->index && row.track == active->track;
  });

  if (it == rows.end() && active->kind == Kind::Track) {
    it = std::find_if(rows.begin(), rows.end(), [&active](const Row& row) {
      return row.kind == Kind::Release && row.index == active->index;
    });
  }

  if (it != rows.end()) menu_->ResetState(static_cast<int>(std::distance(rows.begin(), it)));
}

/* ********************************************************************************************** */

ArtistBrowser::RowState ArtistBrowser::GetRowState(const Row& row) const {
  using Kind = Row::Kind;
  using Mark = internal::YoutubeMenu::Mark;

  if (row.kind != Kind::Release && row.kind != Kind::Track) return RowState{};

  const auto& release = releases_.at(row.index);
  RowState state{.mark = Mark::None, .expanded = release.expanded};

  if (row.kind == Kind::Release && loading_ == row.index) {
    state.detail = std::string{spinner_frame(job_.GetElapsed())};
  }

  if (!release.tracks.has_value() || !playlist_.contains) return state;

  if (row.kind == Kind::Track) {
    state.mark = playlist_.contains(release.tracks->at(row.track)) ? Mark::All : Mark::None;
    return state;
  }

  const auto& tracks = *release.tracks;
  const auto added = std::count_if(tracks.begin(), tracks.end(), playlist_.contains);

  if (added > 0) {
    state.mark = added == static_cast<std::ptrdiff_t>(tracks.size()) ? Mark::All : Mark::Some;
  }
  return state;
}

/* ********************************************************************************************** */

std::string ArtistBrowser::GetHeader() const {
  if (level_ == Level::Releases && channel_.has_value()) {
    const size_t count = releases_.size();
    return channels_.at(*channel_).name + " · " + std::to_string(count) +
           (count == 1 ? " release" : " releases");
  }

  if (level_ == Level::Releases) return "Playlists for \"" + query_ + "\"";

  return "Artists for \"" + query_ + "\"";
}

/* ********************************************************************************************** */

void ArtistBrowser::Refresh() const {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  dispatcher->SendEvent(interface::CustomEvent::Refresh());
}

}  // namespace interface
