#include "view/element/artist_browser.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/custom_event.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"
#include "view/element/util.h"

namespace interface {

namespace {

//! Maximum columns for detail from artist or playlist, shown on the right (so name or title gets
//! most of the space)
constexpr int kMaxDetailColumns = 12;

//! Columns for year in release date (e.g. "2005" from "2005-09-12")
constexpr size_t kYearColumns = 4;

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
    case error::kUrlUnavailable:
      return "MusicBrainz is busy";
    default:
      return fallback;
  }
}

//! Get URL from YouTube playlist (releases are also playlists)
std::string GetPlaylistUrl(const std::string& id) {
  return "https://www.youtube.com/playlist?list=" + id;
}

//! Find track from single with the same title as it (or its first track, when there is none)
int FindSingleTrack(const std::vector<model::Song>& tracks, const std::string& title) {
  for (bool relaxed : {false, true}) {
    auto it = std::find_if(tracks.begin(), tracks.end(), [&](const model::Song& song) {
      return util::same_title(song.title, title, relaxed);
    });

    if (it != tracks.end()) return static_cast<int>(std::distance(tracks.begin(), it));
  }

  return 0;
}

//! Find release from official channel of artist with the same title (ignoring typographic
//! punctuation, and then text in brackets from only one of them, like "(Remastered)")
std::optional<model::YoutubeRelease> FindOfficialRelease(
    const std::vector<model::YoutubeRelease>& official, const std::string& title) {
  for (bool relaxed : {false, true}) {
    auto it = std::find_if(official.begin(), official.end(), [&](const auto& release) {
      return util::same_title(release.title, title, relaxed);
    });

    if (it != official.end()) return *it;
  }

  return std::nullopt;
}

}  // namespace

/* ********************************************************************************************** */

std::pair<ArtistBrowser::Group, std::string> ArtistBrowser::GetReleaseGroup(
    const model::ReleaseGroup& release) {
  if (!release.secondary_types.empty()) {
    const std::string& type = release.secondary_types.front();

    // Shorter names, as there is not much space
    if (type == "Compilation") return {Group::Other, "Comp"};
    if (type == "Soundtrack") return {Group::Other, "OST"};

    return {Group::Other, type};
  }

  if (release.primary_type == "Album") return {Group::Album, ""};
  if (release.primary_type == "EP") return {Group::EP, ""};
  if (release.primary_type == "Single") return {Group::Single, ""};

  return {Group::Other, release.primary_type.empty() ? "Other" : release.primary_type};
}

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
      job_{[this] { Refresh(); }, "search"},
      official_job_{[this] { Refresh(); }, "official"} {}

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
    if (level_ == Level::Releases) {
      hints.push_back(ftxui::text(util::EventToString(Keybind::Space) + ": add/remove"));
    }

    if (level_ == Level::Releases && source_ == Source::Artist) {
      hints.push_back(ftxui::text(util::EventToString(keybinding::Playlist::ShowAllReleases) +
                                  (show_all_ ? ": main types only" : ": all types")));
    }

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

  // Show or hide releases that are not the main ones from artist (e.g. live or compilation)
  if (event == keybinding::Playlist::ShowAllReleases) {
    if (level_ == Level::Releases && source_ == Source::Artist) {
      show_all_ = !show_all_;
      UpdateRows();
    }
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

  // Only open group or release (as Return key would also close it, when it is already open)
  if (event == Keybind::Right || event == Keybind::ArrowRight) {
    if (!GetRowState(*active).expanded && active->kind != Row::Kind::Track) Open(*active);
    return true;
  }

  if (event == Keybind::Left || event == Keybind::ArrowLeft) {
    if (level_ == Level::Releases) Close(*active);
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

  LOG("Search artist=", std::quoted(query));

  StartJob("Searching artists",
           [this, search = fetch_.search_artists,
            query](const std::atomic<bool>* cancel) -> BackgroundJob::Result {
             std::vector<model::Artist> artists;
             error::Code code = search ? search(query, artists, cancel) : error::kUnknownError;

             return [this, code, artists = std::move(artists)] {
               status_.reset();
               searched_ = true;

               if (code != error::kSuccess) {
                 error_ = GetFailureReason(code, "Cannot reach MusicBrainz");
               } else if (artists.empty()) {
                 error_ = "No artists found";
               }

               artists_ = artists;
               UpdateRows();
               menu_->ResetState();
             };
           });

  UpdateRows();
}

/* ********************************************************************************************** */

void ArtistBrowser::Reset() {
  job_.Stop();
  official_job_.Stop();

  level_ = Level::Artists;
  source_ = Source::Artist;
  query_.clear();
  searched_ = false;
  artists_.clear();
  channels_.clear();
  owner_.reset();
  releases_.clear();
  show_all_ = false;
  official_.reset();

  status_.reset();
  loading_.reset();
  error_.reset();

  artist_selected_ = 0;
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
    case Kind::Artist:
      artist_selected_ = row.index;
      OpenArtist(row.index);
      return true;

    case Kind::SearchChannels:
      artist_selected_ = row.index;
      OpenChannels();
      return true;

    case Kind::SearchPlaylists:
      (level_ == Level::Artists ? artist_selected_ : channel_selected_) = row.index;
      OpenYoutubeReleases(std::nullopt);
      return true;

    case Kind::Channel:
      channel_selected_ = row.index;
      OpenYoutubeReleases(row.index);
      return true;

    case Kind::Group: {
      auto& expanded = groups_expanded_.at(static_cast<std::size_t>(row.index));
      expanded = !expanded;
      UpdateRows();
      return true;
    }

    case Kind::Release: {
      auto& release = releases_.at(row.index);

      // Single is added as a song instead
      if (IsSingle(release)) return false;

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

void ArtistBrowser::Close(const Row& row) {
  switch (row.kind) {
    case Row::Kind::Group:
      groups_expanded_.at(static_cast<std::size_t>(row.index)) = false;
      break;

    case Row::Kind::Release: {
      auto& release = releases_.at(row.index);

      if (release.expanded) {
        release.expanded = false;
      } else if (source_ == Source::Artist) {
        groups_expanded_.at(static_cast<std::size_t>(release.group)) = false;
      }
      break;
    }

    case Row::Kind::Track:
      releases_.at(row.index).expanded = false;
      break;

    default:
      return;
  }

  UpdateRows();
}

/* ********************************************************************************************** */

void ArtistBrowser::OpenArtist(int index) {
  const model::Artist artist = artists_.at(index);
  LOG("Open releases from artist=", std::quoted(artist.name), " id=", artist.id);

  StartJob("Loading releases",
           [this, fetch = fetch_.fetch_releases, index,
            artist](const std::atomic<bool>* cancel) -> BackgroundJob::Result {
             std::vector<model::ReleaseGroup> groups;
             error::Code code = fetch ? fetch(artist.id, groups, cancel) : error::kUnknownError;

             return [this, code, index, groups = std::move(groups)] {
               status_.reset();

               if (code != error::kSuccess) {
                 error_ = GetFailureReason(code, "Cannot reach MusicBrainz");
                 official_job_.Stop();
                 return;
               }

               // Stay on artists, so user can pick another one
               if (groups.empty()) {
                 error_ = "No releases from this artist";
                 official_job_.Stop();
                 return;
               }

               level_ = Level::Releases;
               source_ = Source::Artist;
               owner_ = index;
               releases_.clear();

               for (const auto& group : groups) {
                 auto [release_group, type] = GetReleaseGroup(group);
                 releases_.push_back(Release{
                     .id = group.id,
                     .title = group.title,
                     .date = group.date,
                     .type = type,
                     .group = release_group,
                 });
               }

               groups_expanded_.fill(true);

               // Newest first (releases without a date are the last ones)
               std::stable_sort(releases_.begin(), releases_.end(),
                                [](const auto& lhs, const auto& rhs) {
                                  if (lhs.date.empty() || rhs.date.empty())
                                    return !lhs.date.empty() && rhs.date.empty();
                                  return lhs.date > rhs.date;
                                });

               // Artist with only other types of release (e.g. live albums) shows them right away
               show_all_ = std::all_of(releases_.begin(), releases_.end(),
                                       [](const Release& r) { return r.group == Group::Other; });

               UpdateRows();
               menu_->ResetState();
             };
           });

  // Meanwhile, find official channel of artist on YouTube, whose releases are whole albums (much
  // faster than searching each song)
  official_.reset();

  official_job_.Start([this, search = fetch_.search_channels, fetch = fetch_.fetch_channel_releases,
                       name = artist.name](const std::atomic<bool>* cancel) {
    std::vector<model::YoutubeRelease> releases;
    std::vector<model::YoutubeChannel> channels;

    if (search && fetch && search(name, channels, cancel) == error::kSuccess) {
      auto it = std::find_if(channels.begin(), channels.end(), [&name](const auto& channel) {
        return channel.verified && util::same_title(channel.name, name);
      });

      if (it != channels.end() && fetch(it->id, releases, cancel) != error::kSuccess) {
        releases.clear();
      }
    }

    LOG("Found official releases on YouTube=", releases.size(), " for artist=", std::quoted(name));
    return [this, releases = std::move(releases)] { official_ = releases; };
  });
}

/* ********************************************************************************************** */

void ArtistBrowser::OpenChannels() {
  LOG("Search channels on YouTube for artist=", std::quoted(query_));

  StartJob("Searching channels",
           [this, search = fetch_.search_channels,
            query = query_](const std::atomic<bool>* cancel) -> BackgroundJob::Result {
             std::vector<model::YoutubeChannel> channels;
             error::Code code =
                 search ? search(query, channels, cancel) : error::kStreamFetchFailed;

             return [this, code, channels = std::move(channels)] {
               status_.reset();

               if (code != error::kSuccess) {
                 error_ = GetFailureReason(code, "Cannot search channels");
                 return;
               }

               // Stay on artists, so user can search playlists instead
               if (channels.empty()) {
                 error_ = "No channels found";
                 return;
               }

               level_ = Level::Channels;
               channels_ = channels;
               channel_selected_ = 0;

               UpdateRows();
               menu_->ResetState();
             };
           });
}

/* ********************************************************************************************** */

void ArtistBrowser::OpenYoutubeReleases(std::optional<int> channel) {
  const std::string source = channel ? channels_.at(*channel).id : query_;
  auto fetch = channel ? fetch_.fetch_channel_releases : fetch_.search_playlists;

  LOG("Open releases from=", std::quoted(source), " channel=", channel.has_value());

  StartJob(
      channel ? "Loading albums" : "Searching playlists",
      [this, fetch, source, channel](const std::atomic<bool>* cancel) -> BackgroundJob::Result {
        std::vector<model::YoutubeRelease> releases;
        error::Code code = fetch ? fetch(source, releases, cancel) : error::kStreamFetchFailed;

        return [this, code, channel, releases = std::move(releases)] {
          status_.reset();

          if (code != error::kSuccess) {
            error_ =
                GetFailureReason(code, channel ? "Cannot load albums" : "Cannot search playlists");
            return;
          }

          // Stay where it was, so user can pick something else
          if (releases.empty()) {
            error_ = channel ? "No albums on this channel" : "No playlists found";
            return;
          }

          level_ = Level::Releases;
          source_ = channel ? Source::Channel : Source::Playlists;
          owner_ = channel;
          releases_.clear();

          for (const auto& release : releases) {
            releases_.push_back(Release{
                .id = release.id,
                .title = release.title,
                .uploader = channel ? "" : release.uploader,
            });
          }

          UpdateRows();
          menu_->ResetState();
        };
      });
}

/* ********************************************************************************************** */

void ArtistBrowser::Toggle(const Row& row) {
  if (row.kind != Row::Kind::Release && row.kind != Row::Kind::Track) return;

  // Only one thing is fetched at a time
  if (job_.IsRunning()) return;

  const auto& release = releases_.at(row.index);
  std::optional<int> track = row.kind == Row::Kind::Track ? std::optional{row.track} : std::nullopt;

  // Single is added as a song (found once its tracks are known)
  if (IsSingle(release) && release.tracks.has_value()) {
    track = FindSingleTrack(*release.tracks, release.title);
  }

  // Tracks are fetched first, then all of them are added (or only the song from a single)
  if (!release.tracks.has_value()) {
    source_ == Source::Artist ? MatchAndAdd(row.index, std::nullopt) : LoadTracks(row.index, true);
    return;
  }

  std::vector<model::Song> songs = *release.tracks;
  if (track.has_value()) songs = {release.tracks->at(*track)};

  // Tracks not found on YouTube cannot be added (nor removed)
  if (source_ == Source::Artist) {
    std::vector<model::Song> found;

    for (int i = 0; i < static_cast<int>(release.tracks->size()); ++i) {
      if (track.has_value() && i != *track) continue;
      if (!release.missing.at(i)) found.push_back(release.tracks->at(i));
    }

    if (found.empty()) return;
    songs = std::move(found);
  }

  const bool all = std::all_of(songs.begin(), songs.end(), playlist_.contains);
  if (all) {
    LOG("Remove songs from playlist=", songs.size());
    playlist_.remove(songs);
    return;
  }

  // Songs from artist are only added once all of them are found on YouTube
  const bool found = std::all_of(songs.begin(), songs.end(),
                                 [](const model::Song& song) { return song.stream_info; });

  if (!found) {
    MatchAndAdd(row.index, track);
    return;
  }

  LOG("Add songs to playlist=", songs.size());
  playlist_.add(songs);
}

/* ********************************************************************************************** */

void ArtistBrowser::MatchAndAdd(int index, std::optional<int> track) {
  const Release& release = releases_.at(index);
  const std::string artist = artists_.at(*owner_).name;

  LOG("Find songs on YouTube from release=", std::quoted(release.title),
      " track=", track.value_or(-1));

  // Whole release may be found on official channel of artist (once it is known)
  std::optional<std::string> playlist;

  if (!release.official_checked && official_.has_value()) {
    if (auto found = FindOfficialRelease(*official_, release.title); found.has_value()) {
      playlist = GetPlaylistUrl(found->id);
    }
  }

  // Number of songs to find, for progress shown while finding them
  progress_ = 0;
  progress_total_ = 0;

  if (release.tracks.has_value()) {
    for (int i = 0; i < static_cast<int>(release.tracks->size()); ++i) {
      const bool wanted = !track.has_value() || i == *track;
      if (wanted && !release.tracks->at(i).stream_info && !release.missing.at(i)) progress_total_++;
    }
  }

  BackgroundJob::Task task = [this, fetch = fetch_, id = release.id, tracks = release.tracks,
                              missing = release.missing, artist, playlist, index, track,
                              single =
                                  IsSingle(release) ? std::optional{release.title} : std::nullopt](
                                 const std::atomic<bool>* cancel) mutable -> BackgroundJob::Result {
    std::vector<model::Song> songs;
    std::vector<bool> not_found = missing;

    // Tracks from release are known only from MusicBrainz
    if (tracks.has_value()) {
      songs = *tracks;
    } else {
      error::Code code =
          fetch.fetch_tracks ? fetch.fetch_tracks(id, songs, cancel) : error::kUnknownError;

      if (code != error::kSuccess || songs.empty()) {
        return [this, code] {
          loading_.reset();
          error_ = code != error::kSuccess ? GetFailureReason(code, "Cannot reach MusicBrainz")
                                           : "No songs in this release";
        };
      }

      for (auto& song : songs) song.artist = artist;
      not_found.assign(songs.size(), false);

      // Only the song from a single is wanted
      if (single.has_value()) track = FindSingleTrack(songs, *single);
      progress_total_ = static_cast<int>(track.has_value() ? 1 : songs.size());
    }

    // Whole release from official channel, but only when it has the same tracks
    if (playlist.has_value() && fetch.fetch_playlist) {
      std::vector<model::Song> official;

      if (fetch.fetch_playlist(*playlist, official, cancel) == error::kSuccess &&
          official.size() == songs.size()) {
        for (size_t i = 0; i < songs.size(); ++i) {
          if (songs[i].stream_info.has_value()) continue;

          songs[i].stream_info = official[i].stream_info;
          if (songs[i].duration == 0) songs[i].duration = official[i].duration;
          not_found[i] = false;
          ++progress_;
        }
      }
    }

    // Otherwise, find each song (only the ones wanted, which were not searched before)
    std::vector<size_t> pending;
    std::vector<model::Song> search;

    for (size_t i = 0; i < songs.size(); ++i) {
      const bool wanted = !track.has_value() || static_cast<int>(i) == *track;
      if (!wanted || songs[i].stream_info.has_value() || not_found[i]) continue;

      pending.push_back(i);
      search.push_back(songs[i]);
    }

    error::Code code = error::kSuccess;

    if (!search.empty()) {
      code = fetch.match_songs ? fetch.match_songs(search, &progress_, cancel)
                               : error::kStreamFetcherNotFound;

      for (size_t i = 0; i < pending.size(); ++i) {
        songs[pending[i]].stream_info = search[i].stream_info;

        // Song was searched but not found (search was not refused or stopped before it)
        if (code == error::kSuccess && !search[i].stream_info) not_found[pending[i]] = true;
      }
    }

    return [this, index, track, code, checked = playlist.has_value(), songs = std::move(songs),
            not_found = std::move(not_found)] {
      loading_.reset();
      progress_total_ = 0;

      auto& release = releases_.at(index);
      release.tracks = songs;
      release.missing = not_found;
      release.official_checked = release.official_checked || checked;

      if (code != error::kSuccess) error_ = GetFailureReason(code, "Cannot search YouTube");

      // Add songs found, keeping their order from release
      std::vector<model::Song> found;

      for (size_t i = 0; i < songs.size(); ++i) {
        const bool wanted = !track.has_value() || static_cast<int>(i) == *track;
        if (wanted && songs[i].stream_info.has_value()) found.push_back(songs[i]);
      }

      if (found.empty() && !error_.has_value()) error_ = "Not found on YouTube";

      LOG("Add songs found on YouTube=", found.size(), " total=", songs.size());
      if (!found.empty()) playlist_.add(found);

      UpdateRows();
    };
  };

  loading_ = index;
  error_.reset();
  status_.reset();
  job_.Start(task);
}

/* ********************************************************************************************** */

void ArtistBrowser::Back() {
  // Stop what is being fetched, staying where it was started from
  if (job_.IsRunning()) {
    LOG("Stop fetching from artist browser");
    job_.Stop();

    status_.reset();
    loading_.reset();
    progress_total_ = 0;

    // Search itself was stopped, so there is nothing to show
    if (level_ == Level::Artists && !searched_) playlist_.back();

    UpdateRows();
    return;
  }

  error_.reset();

  switch (level_) {
    case Level::Artists:
      playlist_.back();
      return;

    case Level::Channels:
      level_ = Level::Artists;
      channels_.clear();

      UpdateRows();
      menu_->ResetState(artist_selected_);
      return;

    case Level::Releases: {
      // Releases from channel go back to channels, everything else goes back to artists
      const bool channel =
          source_ == Source::Channel || (source_ == Source::Playlists && !channels_.empty());

      level_ = channel ? Level::Channels : Level::Artists;
      owner_.reset();
      releases_.clear();
      show_all_ = false;

      official_job_.Stop();
      official_.reset();

      UpdateRows();
      menu_->ResetState(channel ? channel_selected_ : artist_selected_);
      return;
    }
  }
}

/* ********************************************************************************************** */

void ArtistBrowser::LoadTracks(int index, bool add) {
  // Only one thing is fetched at a time
  if (job_.IsRunning()) return;

  const bool from_artist = source_ == Source::Artist;
  const auto& release = releases_.at(index);

  // Release from artist is listed by MusicBrainz, otherwise it is a playlist on YouTube
  const std::string id = from_artist ? release.id : GetPlaylistUrl(release.id);
  auto fetch = from_artist ? fetch_.fetch_tracks : fetch_.fetch_playlist;
  const std::string artist = from_artist ? artists_.at(*owner_).name : "";

  LOG("Load tracks from release=", std::quoted(id), " add=", add);

  loading_ = index;

  BackgroundJob::Task task = [this, fetch, id, index, add, artist, from_artist](
                                 const std::atomic<bool>* cancel) -> BackgroundJob::Result {
    std::vector<model::Song> songs;
    error::Code code = fetch ? fetch(id, songs, cancel) : error::kStreamFetchFailed;

    return [this, code, index, add, artist, from_artist, songs = std::move(songs)]() mutable {
      loading_.reset();

      if (code != error::kSuccess || songs.empty()) {
        const std::string fallback = from_artist ? "Cannot reach MusicBrainz" : "Cannot load album";
        error_ =
            code != error::kSuccess ? GetFailureReason(code, fallback) : "No songs in this album";
        return;
      }

      if (from_artist) {
        for (auto& song : songs) song.artist = artist;
      }

      auto& release = releases_.at(index);
      release.tracks = songs;
      release.missing.assign(songs.size(), false);

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

  // Options to search YouTube instead (e.g. artist unknown by MusicBrainz)
  auto add_fallbacks = [this, &rows](bool channels) {
    if (query_.empty() || status_.has_value()) return;

    if (channels) {
      rows.push_back(Row{.kind = Kind::SearchChannels,
                         .index = static_cast<int>(rows.size()),
                         .text = "Search YouTube channels"});
    }

    rows.push_back(Row{.kind = Kind::SearchPlaylists,
                       .index = static_cast<int>(rows.size()),
                       .text = "Search playlists for \"" + query_ + "\""});
  };

  switch (level_) {
    case Level::Artists:
      for (int i = 0; i < static_cast<int>(artists_.size()); ++i) {
        const auto& artist = artists_.at(i);

        // Artists with the same name are told apart by a comment, otherwise it is better to show
        // what they are and where they come from (comments are too long for the space available)
        const bool homonym =
            std::any_of(artists_.begin(), artists_.end(), [&artist](const auto& other) {
              return &other != &artist && util::same_title(other.name, artist.name);
            });

        std::string detail = homonym ? artist.disambiguation : "";
        if (detail.empty()) {
          detail = artist.type;
          if (!detail.empty() && !artist.country.empty()) detail += " · ";
          detail += artist.country;
        }

        rows.push_back(Row{.kind = Kind::Artist,
                           .index = i,
                           .text = artist.name,
                           .detail = ellipsize(detail, kMaxDetailColumns)});
      }

      // Searching artists may fail (e.g. without network), but YouTube may still be searched
      if (searched_) add_fallbacks(true);
      break;

    case Level::Channels:
      for (int i = 0; i < static_cast<int>(channels_.size()); ++i) {
        const auto& channel = channels_.at(i);
        rows.push_back(Row{
            .kind = Kind::Channel,
            .index = i,
            .text = channel.name + (channel.verified ? " ✓" : ""),
            .detail = channel.followers ? FormatFollowers(*channel.followers) : "",
        });
      }

      add_fallbacks(false);
      break;

    case Level::Releases: {
      // Add release (and its tracks below it, when expanded) at the given indentation level
      auto add_release = [this, &rows](int i, int depth) {
        const auto& release = releases_.at(i);
        std::string detail;

        switch (source_) {
          case Source::Artist:
            // Type is shown only for other types (e.g. "Live 2011"), as group tells the others
            detail = release.date.substr(0, kYearColumns);
            if (!release.type.empty()) detail = release.type + (detail.empty() ? "" : " ") + detail;
            break;

          case Source::Channel:
            if (release.tracks.has_value()) detail = std::to_string(release.tracks->size());
            break;

          case Source::Playlists:
            // Playlist created by anyone tells who did it, as it may not be from the artist
            detail = ellipsize(release.uploader, kMaxDetailColumns);
            break;
        }

        const bool single = IsSingle(release);

        rows.push_back(Row{.kind = Kind::Release,
                           .index = i,
                           .text = release.title,
                           .detail = detail,
                           .depth = depth,
                           .leaf = single});

        if (single || !release.expanded || !release.tracks.has_value()) return;

        for (int t = 0; t < static_cast<int>(release.tracks->size()); ++t) {
          const auto& song = release.tracks->at(t);

          // Tracks from a release are all from the same artist (unlike any other playlist)
          const std::string title =
              source_ != Source::Playlists && !song.title.empty() ? song.title : song.GetTitle();

          rows.push_back(Row{.kind = Kind::Track,
                             .index = i,
                             .track = t,
                             .text = title,
                             .detail = FormatDuration(song.duration),
                             .depth = depth + 1});
        }
      };

      if (source_ != Source::Artist) {
        for (int i = 0; i < static_cast<int>(releases_.size()); ++i) add_release(i, 0);
        break;
      }

      // Releases from artist are grouped by type, in the same order as groups are declared
      static constexpr std::array<std::string_view, kGroups> kGroupNames{"Albums", "EPs", "Singles",
                                                                         "Other"};

      for (std::size_t g = 0; g < kGroups; ++g) {
        std::vector<int> members;

        for (int i = 0; i < static_cast<int>(releases_.size()); ++i) {
          const auto& release = releases_.at(i);
          if (release.group == static_cast<Group>(g) && IsVisible(release)) members.push_back(i);
        }

        if (members.empty()) continue;

        rows.push_back(Row{
            .kind = Kind::Group,
            .index = static_cast<int>(g),
            .text = std::string{kGroupNames[g]} + " (" + std::to_string(members.size()) + ")",
        });

        if (!groups_expanded_[g]) continue;
        for (int i : members) add_release(i, 1);
      }
      break;
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

  // Or its group, when releases from it are not shown anymore
  const bool in_group = (active->kind == Kind::Release || active->kind == Kind::Track) &&
                        level_ == Level::Releases && source_ == Source::Artist &&
                        active->index < static_cast<int>(releases_.size());

  if (it == rows.end() && in_group) {
    const auto group = static_cast<int>(releases_.at(active->index).group);
    it = std::find_if(rows.begin(), rows.end(), [group](const Row& row) {
      return row.kind == Kind::Group && row.index == group;
    });
  }

  if (it != rows.end()) menu_->ResetState(static_cast<int>(std::distance(rows.begin(), it)));
}

/* ********************************************************************************************** */

ArtistBrowser::RowState ArtistBrowser::GetRowState(const Row& row) const {
  using Kind = Row::Kind;
  using Mark = internal::YoutubeMenu::Mark;

  if (row.kind == Kind::Group) {
    return RowState{.expanded = groups_expanded_.at(static_cast<std::size_t>(row.index))};
  }

  if (row.kind != Kind::Release && row.kind != Kind::Track) return RowState{};

  const auto& release = releases_.at(row.index);
  RowState state{.mark = Mark::None, .expanded = release.expanded};

  // Release being loaded shows a spinner (and how many songs were found, while finding them)
  if (row.kind == Kind::Release && loading_ == row.index) {
    std::string detail{spinner_frame(job_.GetElapsed())};
    if (progress_total_ > 0) {
      detail += " " + std::to_string(progress_) + "/" + std::to_string(progress_total_);
    }

    state.detail = detail;
  }

  if (!release.tracks.has_value() || !playlist_.contains) return state;

  const auto& tracks = *release.tracks;
  auto is_missing = [&release](size_t i) {
    return i < release.missing.size() && release.missing[i];
  };

  // Single is marked as the song added from it
  const bool single = row.kind == Kind::Release && IsSingle(release);

  if (row.kind == Kind::Track || single) {
    const auto index =
        static_cast<size_t>(single ? FindSingleTrack(tracks, release.title) : row.track);

    if (is_missing(index)) {
      state.mark = Mark::Missing;
    } else if (playlist_.contains(tracks.at(index))) {
      state.mark = Mark::All;
    }

    return state;
  }

  // Tracks not found on YouTube cannot be added, so they are not counted
  std::ptrdiff_t added = 0;
  std::ptrdiff_t available = 0;

  for (size_t i = 0; i < tracks.size(); ++i) {
    if (is_missing(i)) continue;

    available++;
    if (playlist_.contains(tracks[i])) added++;
  }

  if (added > 0) state.mark = added == available ? Mark::All : Mark::Some;
  return state;
}

/* ********************************************************************************************** */

std::string ArtistBrowser::GetHeader() const {
  if (level_ == Level::Releases && source_ != Source::Playlists && owner_.has_value()) {
    const std::string& name =
        source_ == Source::Artist ? artists_.at(*owner_).name : channels_.at(*owner_).name;
    const int count = CountVisibleReleases();

    return name + " · " + std::to_string(count) + (count == 1 ? " release" : " releases");
  }

  if (level_ == Level::Releases) return "Playlists for \"" + query_ + "\"";
  if (level_ == Level::Channels) return "Channels for \"" + query_ + "\"";

  return "Artists for \"" + query_ + "\"";
}

/* ********************************************************************************************** */

int ArtistBrowser::CountVisibleReleases() const {
  return static_cast<int>(std::count_if(releases_.begin(), releases_.end(),
                                        [this](const auto& r) { return IsVisible(r); }));
}

/* ********************************************************************************************** */

void ArtistBrowser::Refresh() const {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return;

  dispatcher->SendEvent(interface::CustomEvent::Refresh());
}

}  // namespace interface
