/**
 * \file
 * \brief  Class for browsing releases from an artist on YouTube, to add their songs to a playlist
 */

#ifndef INCLUDE_VIEW_ELEMENT_ARTIST_BROWSER_H_
#define INCLUDE_VIEW_ELEMENT_ARTIST_BROWSER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/application_error.h"
#include "model/song.h"
#include "model/youtube_search.h"
#include "view/base/element.h"
#include "view/base/event_dispatcher.h"
#include "view/element/background_job.h"
#include "view/element/menu.h"

namespace interface {

/**
 * @brief Browse channels found for an artist on YouTube, then releases from the chosen channel (or
 * playlists found for artist, when there is no channel with releases), adding whole releases or
 * single tracks to the playlist being edited. Everything is fetched in another thread, only when
 * needed (each step takes about a second)
 */
class ArtistBrowser : public Element {
  static constexpr int kPadding = 1;  //!< Columns left empty on each side of content

  //! Minimum lines to show hints for keys (otherwise, they would take most of the space)
  static constexpr int kMinLinesForHints = 16;

 public:
  //! Callback to search channels from artist (cancel flag is set to stop search)
  using SearchArtistsCallback =
      std::function<error::Code(const std::string& query, std::vector<model::YoutubeChannel>&,
                                const std::atomic<bool>* cancel)>;

  //! Callback to list releases from channel, or to search playlists from artist (cancel flag is
  //! set to stop it)
  using ReleasesCallback =
      std::function<error::Code(const std::string& channel_id_or_query,
                                std::vector<model::YoutubeRelease>&, const std::atomic<bool>*)>;

  //! Callback to extract songs from a playlist URL (cancel flag is set to stop extraction)
  using TracksCallback = std::function<error::Code(
      const std::string& url, std::vector<model::Song>& songs, const std::atomic<bool>* cancel)>;

  //! Functions to fetch everything from YouTube
  struct FetchCallbacks {
    SearchArtistsCallback search_artists;  //!< Search channels from artist
    ReleasesCallback fetch_releases;       //!< List releases from channel
    ReleasesCallback search_playlists;     //!< Search playlists from artist
    TracksCallback fetch_tracks;           //!< List songs from release or playlist
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
   * @param fetch Functions to fetch everything from YouTube
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
   * @brief Start searching channels from the given artist (forgetting anything from before)
   * @param query Artist name
   */
  void Search(const std::string& query);

  /**
   * @brief Apply result from what was being fetched, if it has finished (must be called on every
   * event, as UI is asked to refresh when it finishes)
   */
  void Update() { job_.Finish(); }

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
    Channels,  //!< Channels found for artist (and option to search playlists instead)
    Releases,  //!< Releases from channel (or playlists found for artist), with their tracks
  };

  //! Release (or playlist) and its tracks, once they are fetched
  struct Release {
    model::YoutubeRelease info;                      //!< Release information
    std::optional<std::vector<model::Song>> tracks;  //!< Tracks (fetched when release is opened)
    bool expanded = false;                           //!< Tracks are shown below release
  };

  //! Open entry: channel shows its releases, release shows its tracks (or hides them)
  bool Open(const Row& row);

  //! Add release or track to playlist (or remove it, when it is already there)
  void Toggle(const Row& row);

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

  //! Ask for a UI refresh
  void Refresh() const;

  /* ******************************************************************************************** */
  //! Variables

  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks

  FetchCallbacks fetch_;        //!< Functions to fetch everything from YouTube
  PlaylistCallbacks playlist_;  //!< Functions to change the playlist being edited

  Level level_ = Level::Channels;                //!< What is listed
  std::string query_;                            //!< Artist searched
  std::vector<model::YoutubeChannel> channels_;  //!< Channels found for artist
  std::optional<int> channel_;     //!< Channel whose releases are listed (none for playlists found)
  std::vector<Release> releases_;  //!< Releases from channel (or playlists found for artist)

  std::optional<std::string> status_;  //!< What is being fetched (shown next to a spinner)
  std::optional<int> loading_;         //!< Release whose tracks are being fetched
  std::optional<std::string> error_;   //!< Why the last thing could not be fetched (or was empty)

  int channel_selected_ = 0;  //!< Channel selected before opening it (selected again when back)

  int max_lines_ = 0;  //!< Maximum lines available to render content

  YoutubeMenu menu_;   //!< Menu listing channels, or releases and their tracks
  BackgroundJob job_;  //!< Fetch something in another thread (only one at a time)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_ARTIST_BROWSER_H_
