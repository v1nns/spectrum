/**
 * \file
 * \brief  Class for tab view containing song lyrics
 */

#ifndef INCLUDE_VIEW_BLOCK_MAIN_CONTENT_SONG_LYRIC_H_
#define INCLUDE_VIEW_BLOCK_MAIN_CONTENT_SONG_LYRIC_H_

#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <string_view>

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
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
  //! Private methods
 private:
  /**
   * @brief Check inner state from std::future
   * @tparam R Result from asynchronous operation
   * @param f Mechanism to execute asynchronous operation
   * @param st Inner state
   * @return true if state matches, otherwise false
   */
  template <typename R>
  bool is_state(std::future<R>& f, std::future_status st) const {
    return f.valid() && f.wait_for(std::chrono::seconds(0)) == st;
  }

  /**
   * @brief Check state from fetch operation that is executed asynchronously
   * @return true if fetch operation is still executing, otherwise false
   */
  bool IsFetching() {
    return async_fetcher_ && is_state(*async_fetcher_, std::future_status::timeout);
  }

  /**
   * @brief Check state from fetch operation that is executed asynchronously
   * @return true if fetch operation finished, otherwise false
   */
  bool IsResultReady() {
    return async_fetcher_ && is_state(*async_fetcher_, std::future_status::ready);
  }

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

  std::unique_ptr<lyric::LyricFinder> finder_ = lyric::LyricFinder::Create();  //!< Lyric finder
  std::unique_ptr<std::future<lyric::SearchResult>> async_fetcher_;  //!< Search asynchronously

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::MainContentTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_MAIN_CONTENT_SONG_LYRIC_H_
