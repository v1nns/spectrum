/**
 * \file
 * \brief  Class for tab view containing song lyrics
 */

#ifndef INCLUDE_VIEW_BLOCK_MAIN_CONTENT_SONG_LYRIC_H_
#define INCLUDE_VIEW_BLOCK_MAIN_CONTENT_SONG_LYRIC_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "audio/lyric/lyric_finder.h"
#include "model/song.h"
#include "view/element/tab.h"

#ifdef ENABLE_TESTS
namespace {
class MainContentTest;
}
#endif

namespace interface {

/**
 * @brief Component to render lyric from current song
 */
class SongLyric : public TabItem {
  static constexpr std::string_view kTabName = "lyric";  //!< Tab title

 public:
  /**
   * @brief Construct a new SongLyric object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   * @param on_focus Callback function to ask for focus
   * @param keybinding Keybinding to set item as active
   */
  explicit SongLyric(const model::BlockIdentifier& id,
                     const std::shared_ptr<EventDispatcher>& dispatcher,
                     const FocusCallback& on_focus, const keybinding::Key& keybinding);

  /**
   * @brief Destroy the SongLyric object
   */
  ~SongLyric() override;

  /**
   * @brief Renders the component
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
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
  //! Private methods
 private:
  /**
   * @brief Check if song lyrics are being fetched (search result not received yet)
   * @return true if fetch operation is still executing, otherwise false
   */
  [[nodiscard]] bool IsFetching() const { return fetching_; }

  /**
   * @brief Take search result from fetcher thread (if available) and update song lyrics
   */
  void ConsumeSearchResult();

  /**
   * @brief Cancel search in progress (if any) and discard its result
   */
  void CancelFetching();

  /**
   * @brief Thread loop to search for song lyrics (one request at a time), so UI thread never waits
   * for it
   */
  void FetchLoop();

  /**
   * @brief Get artist and title to search for song lyrics, from song metadata or its filename
   * (following the pattern "artist - title.ext"). If not possible, both are left empty
   */
  void ParseSearchTerms();

  /**
   * @brief Check if song information contains enough data to search for song lyrics
   * @return true if both artist and title are known, otherwise false
   */
  [[nodiscard]] bool HasSearchTerms() const { return !artist_.empty() && !title_.empty(); }

  /**
   * @brief Launch asynchronous task to search for song lyrics using current artist and title
   */
  void StartFetching();

  /**
   * @brief Check if user may retry fetching song lyrics (only when last attempt did not find them)
   * @return true if retry is possible, otherwise false
   */
  bool CanRetry();

  /**
   * @brief Renders message explaining why song lyrics are not being shown
   * @return UI element
   */
  [[nodiscard]] ftxui::Element DrawFailure() const;

  /**
   * @brief Renders the song lyrics element
   * @param lyrics Song lyrics (each entry represents a paragraph)
   */
  ftxui::Element DrawSongLyrics(const model::SongLyric& lyrics) const;

  /* ******************************************************************************************** */
  //! Variables

  model::Song audio_info_;   //!< Audio information from current song
  model::SongLyric lyrics_;  //!< Song lyrics from current song

  std::string artist_;  //!< Artist used to search for song lyrics
  std::string title_;   //!< Title used to search for song lyrics

  std::optional<lyric::SearchResult::Status> status_;  //!< Outcome from last search
  int focused_ = 0;  //!< Index for paragraph focused from song lyric
  ftxui::Box box_;   //!< Box to control if mouse cursor is over song lyric

  std::unique_ptr<lyric::LyricFinder> finder_ = lyric::LyricFinder::Create();  //!< Lyric finder

  //! Request to search for song lyrics
  struct Request {
    uint64_t id;         //!< Request identifier (results from older requests are discarded)
    std::string artist;  //!< Artist to search
    std::string title;   //!< Title to search
  };

  bool fetching_ = false;  //!< Flag to indicate that search result was not received yet

  std::mutex mutex_;                           //!< Control access to fetcher thread data
  std::condition_variable notifier_;           //!< Wake up fetcher thread on new request or exit
  std::optional<Request> pending_;             //!< Request waiting to be executed
  std::optional<lyric::SearchResult> result_;  //!< Result from latest request
  std::atomic<uint64_t> request_id_ = 0;       //!< Identifier from latest request
  std::atomic<bool> exit_ = false;             //!< Flag to stop fetcher thread

  std::thread fetcher_;  //!< Thread to search for song lyrics (declared last, started last)

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::MainContentTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_MAIN_CONTENT_SONG_LYRIC_H_
