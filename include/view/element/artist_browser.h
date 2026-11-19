/**
 * \file
 * \brief  Class for browsing releases from an artist, to add their songs to a playlist
 */

#ifndef INCLUDE_VIEW_ELEMENT_ARTIST_BROWSER_H_
#define INCLUDE_VIEW_ELEMENT_ARTIST_BROWSER_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/application_error.h"
#include "model/artist_search.h"
#include "model/song.h"
#include "view/base/element.h"
#include "view/base/event_dispatcher.h"
#include "view/element/background_job.h"
#include "view/element/menu.h"

namespace interface {

/**
 * @brief Browse artists found on MusicBrainz, then releases from the chosen artist (grouped by
 * type, newest first, with release year), adding whole releases or single tracks to the playlist
 * being edited (a single is added as a song, without listing its tracks). Each
 * song is found on YouTube only when added (whole releases from the official channel of artist,
 * when it has the same release). Artists unknown by MusicBrainz may still be searched on YouTube:
 * channels found (with their releases) or playlists found. Everything is fetched in another
 * thread, only when needed (each step takes about a second)
 */
class ArtistBrowser : public Element {
  static constexpr int kPadding = 1;  //!< Columns left empty on each side of content

  //! Minimum lines to show hints for keys (otherwise, they would take most of the space)
  static constexpr int kMinLinesForHints = 16;

 public:
  //! Callback to search artists on MusicBrainz (cancel flag is set to stop search)
  using ArtistsCallback = std::function<error::Code(
      const std::string& query, std::vector<model::Artist>&, const std::atomic<bool>* cancel)>;

  //! Callback to list releases from artist on MusicBrainz (cancel flag is set to stop it)
  using ReleasesCallback =
      std::function<error::Code(const std::string& artist_id, std::vector<model::ReleaseGroup>&,
                                const std::atomic<bool>* cancel)>;

  //! Callback to find songs on YouTube, filling URL of each one found (cancel flag is set to stop
  //! it, and progress tells how many songs were already searched)
  using MatchCallback =
      std::function<error::Code(std::vector<model::Song>& songs, std::atomic<int>* progress,
                                const std::atomic<bool>* cancel)>;

  //! Callback to search channels from artist on YouTube (cancel flag is set to stop search)
  using ChannelsCallback =
      std::function<error::Code(const std::string& query, std::vector<model::YoutubeChannel>&,
                                const std::atomic<bool>* cancel)>;

  //! Callback to list releases from channel, or to search playlists from artist, on YouTube
  //! (cancel flag is set to stop it)
  using YoutubeReleasesCallback =
      std::function<error::Code(const std::string& channel_id_or_query,
                                std::vector<model::YoutubeRelease>&, const std::atomic<bool>*)>;

  //! Callback to list songs from release, by its identifier on MusicBrainz or its playlist URL on
  //! YouTube (cancel flag is set to stop it)
  using TracksCallback = std::function<error::Code(
      const std::string& id_or_url, std::vector<model::Song>& songs, const std::atomic<bool>*)>;

  //! Functions to fetch everything from MusicBrainz and YouTube
  struct FetchCallbacks {
    ArtistsCallback search_artists;   //!< Search artists on MusicBrainz
    ReleasesCallback fetch_releases;  //!< List releases from artist on MusicBrainz
    TracksCallback fetch_tracks;      //!< List tracks from release on MusicBrainz
    MatchCallback match_songs;        //!< Find songs on YouTube

    ChannelsCallback search_channels;                //!< Search channels on YouTube
    YoutubeReleasesCallback fetch_channel_releases;  //!< List releases from channel on YouTube
    YoutubeReleasesCallback search_playlists;        //!< Search playlists on YouTube
    TracksCallback fetch_playlist;                   //!< List songs from playlist on YouTube
  };

  //! Functions to change the playlist being edited (owned by someone else)
  struct PlaylistCallbacks {
    std::function<bool(const model::Song&)> contains;             //!< Check if song is in playlist
    std::function<void(const std::vector<model::Song>&)> add;     //!< Add songs (if not there yet)
    std::function<void(const std::vector<model::Song>&)> remove;  //!< Remove songs
    std::function<void()> back;  //!< User left browser (going back to where the search started)
  };

  /**
   * @brief Construct a new ArtistBrowser object
   * @param dispatcher Event dispatcher
   * @param fetch Functions to fetch everything from MusicBrainz and YouTube
   * @param playlist Functions to change the playlist being edited
   */
  ArtistBrowser(const std::shared_ptr<EventDispatcher>& dispatcher, const FetchCallbacks& fetch,
                const PlaylistCallbacks& playlist);

  /**
   * @brief Destroy ArtistBrowser object
   */
  ~ArtistBrowser() override = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Renders the element
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event& event) override;

  /**
   * @brief Start searching the given artist (forgetting anything from before)
   * @param query Artist name
   */
  void Search(const std::string& query);

  /**
   * @brief Apply results from what was being fetched, if it has finished (must be called on every
   * event, as UI is asked to refresh when it finishes)
   */
  void Update() {
    job_.Finish();
    official_job_.Finish();
  }

  /**
   * @brief Stop fetching anything and forget everything from the last search
   */
  void Reset();

  /**
   * @brief Set maximum number of columns available to render content
   * @param max_columns Number of columns
   */
  void SetMaxColumns(int max_columns);

  /**
   * @brief Set maximum number of lines available to render content
   * @param max_lines Number of lines
   */
  void SetMaxLines(int max_lines) { max_lines_ = max_lines; }

  /* ******************************************************************************************** */
  //! Internal implementation
 private:
  using Row = internal::YoutubeMenu::Row;
  using RowState = internal::YoutubeMenu::RowState;

  //! What is listed
  enum class Level : std::uint8_t {
    Artists,   //!< Artists found on MusicBrainz (and options to search YouTube instead)
    Channels,  //!< Channels found for artist on YouTube (and option to search playlists instead)
    Releases,  //!< Releases from artist or channel (or playlists found), with their tracks
  };

  //! Where releases come from
  enum class Source : std::uint8_t {
    Artist,     //!< Artist on MusicBrainz (songs are found on YouTube when added)
    Channel,    //!< Channel on YouTube
    Playlists,  //!< Playlists found on YouTube
  };

  //! Group of releases from artist on MusicBrainz, listed in this order
  enum class Group : std::uint8_t {
    Album,   //!< Albums
    EP,      //!< EPs
    Single,  //!< Singles (each one added as a song, without listing its tracks)
    Other,   //!< Any other type, only shown when all types are (e.g. live or compilation)
  };

  //! Number of groups of releases
  static constexpr std::size_t kGroups = static_cast<std::size_t>(Group::Other) + 1;

  //! Release (or playlist) and its tracks, once they are fetched
  struct Release {
    std::string id;        //!< Release identifier on MusicBrainz, or playlist identifier on YouTube
    std::string title;     //!< Release title
    std::string date;      //!< First release date (only from MusicBrainz)
    std::string type;      //!< Release type shown before its year (only for other types)
    std::string uploader;  //!< Who created playlist (only for playlists found)
    Group group = Group::Album;  //!< Group of release (only from MusicBrainz)

    std::optional<std::vector<model::Song>> tracks;  //!< Tracks (fetched when release is opened)
    std::vector<bool> missing;      //!< Track was not found on YouTube (only for artist releases)
    bool official_checked = false;  //!< Release was already searched on artist channel
    bool expanded = false;          //!< Tracks are shown below release
  };

  //! Open entry: artist or channel shows its releases, group shows its releases and release shows
  //! its tracks (or hides them, when they are shown)
  bool Open(const Row& row);

  //! Close entry: group hides its releases and release hides its tracks (from any of its tracks
  //! too), and a release already closed (or a single) closes its group
  void Close(const Row& row);

  //! Get group of release from artist, and its type shown before its year (only for other types)
  static std::pair<Group, std::string> GetReleaseGroup(const model::ReleaseGroup& release);

  //! Check if release is a single from artist (added as a song, without listing its tracks)
  bool IsSingle(const Release& release) const {
    return source_ == Source::Artist && release.group == Group::Single;
  }

  //! Check if release is shown (other types are hidden unless all types are shown)
  bool IsVisible(const Release& release) const {
    return show_all_ || release.group != Group::Other;
  }

  //! Show releases from artist on MusicBrainz (and find artist channel on YouTube meanwhile)
  void OpenArtist(int index);

  //! Show channels found on YouTube
  void OpenChannels();

  //! Show releases from channel on YouTube, or playlists found on YouTube (without a channel)
  void OpenYoutubeReleases(std::optional<int> channel);

  //! Add release or track to playlist (or remove it, when it is already there)
  void Toggle(const Row& row);

  //! Find songs from release on YouTube, then add them to playlist (only the given track, if any)
  void MatchAndAdd(int index, std::optional<int> track);

  //! Go back one step (stopping what is being fetched, if anything)
  void Back();

  //! Fetch tracks from release, then expand it (or add all of them to playlist)
  void LoadTracks(int index, bool add);

  /**
   * @brief Start fetching something in another thread
   * @param status Text shown (next to a spinner) while it is running
   * @param task Function to fetch it, which returns another function to apply its result
   */
  void StartJob(const std::string& status, const BackgroundJob::Task& task);

  //! Update menu entries from current state, keeping the same entry selected (when it still exists)
  void UpdateRows();

  //! Get current state from entry (e.g. how many of its songs are in playlist)
  RowState GetRowState(const Row& row) const;

  //! Get text shown above menu
  std::string GetHeader() const;

  //! Get number of releases shown
  int CountVisibleReleases() const;

  //! Ask for a UI refresh
  void Refresh() const;

  /* ******************************************************************************************** */
  //! Variables

  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks

  FetchCallbacks fetch_;        //!< Functions to fetch everything from MusicBrainz and YouTube
  PlaylistCallbacks playlist_;  //!< Functions to change the playlist being edited

  Level level_ = Level::Artists;                 //!< What is listed
  Source source_ = Source::Artist;               //!< Where releases come from
  std::string query_;                            //!< Artist searched
  bool searched_ = false;                        //!< Search has finished (even if nothing found)
  std::vector<model::Artist> artists_;           //!< Artists found on MusicBrainz
  std::vector<model::YoutubeChannel> channels_;  //!< Channels found on YouTube
  std::optional<int> owner_;                     //!< Artist or channel whose releases are listed
  std::vector<Release> releases_;  //!< Releases from artist or channel (or playlists found)
  bool show_all_ = false;          //!< Show every release type (e.g. live or compilation)

  //! Groups showing their releases (only for releases from artist on MusicBrainz)
  std::array<bool, kGroups> groups_expanded_{};

  //! Releases from official channel of artist on YouTube (nothing while they are being fetched)
  std::optional<std::vector<model::YoutubeRelease>> official_;

  std::optional<std::string> status_;  //!< What is being fetched (shown next to a spinner)
  std::optional<int> loading_;         //!< Release whose tracks are being fetched (or found)
  std::optional<std::string> error_;   //!< Why the last thing could not be fetched (or was empty)

  std::atomic<int> progress_ = 0;        //!< Songs already searched on YouTube
  std::atomic<int> progress_total_ = 0;  //!< Songs being searched on YouTube

  int artist_selected_ = 0;   //!< Entry selected in artists before opening it (selected when back)
  int channel_selected_ = 0;  //!< Entry selected in channels before opening it (selected when back)

  int max_lines_ = 0;  //!< Maximum lines available to render content

  YoutubeMenu menu_;            //!< Menu listing artists or channels, or releases and their tracks
  BackgroundJob job_;           //!< Fetch something in another thread (only one at a time)
  BackgroundJob official_job_;  //!< Fetch releases from official channel of artist on YouTube
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_ARTIST_BROWSER_H_
