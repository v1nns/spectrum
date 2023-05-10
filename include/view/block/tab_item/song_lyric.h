/**
 * \file
 * \brief  Class for tab view containing song lyrics
 */

#ifndef INCLUDE_VIEW_BLOCK_TAB_ITEM_SONG_LYRIC_H_
#define INCLUDE_VIEW_BLOCK_TAB_ITEM_SONG_LYRIC_H_

#include <chrono>
#include <future>
#include <optional>

#include "audio/lyric/base/html_parser.h"
#include "audio/lyric/base/url_fetcher.h"
#include "audio/lyric/lyric_finder.h"
#include "model/song.h"
#include "view/element/tab_item.h"

#ifdef ENABLE_TESTS
namespace {
class TabViewerTest;
}
#endif

namespace interface {

/**
 * @brief Component to render lyric from current song
 */
class SongLyric : public TabItem {
 public:
  /**
   * @brief Construct a new SongLyric object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   */
  explicit SongLyric(const model::BlockIdentifier& id,
                     const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the SongLyric object
   */
  ~SongLyric() override = default;

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
  bool is_state(std::future<R>& f, std::future_status st) {
    return f.valid() && f.wait_for(std::chrono::seconds(0)) == st;
  }

  /**
   * @brief Check state from fetch operation that is executed asynchronously
   * @return true if fetch operation is still executing, otherwise false
   */
  bool IsFetching() { return is_state(async_fetcher_, std::future_status::timeout); }

  /**
   * @brief Check state from fetch operation that is executed asynchronously
   * @return true if fetch operation finished, otherwise false
   */
  bool IsResultReady() { return is_state(async_fetcher_, std::future_status::ready); }

  //! Result from asynchronous fetch operation (if empty, it means that failed)
  using FetchResult = std::optional<lyric::SongLyric>;

  /**
   * @brief Use updated song information to fetch song lyrics
   */
  FetchResult FetchSongLyrics();

  /**
   * @brief Renders the song lyrics element
   * @param lyrics Song lyrics (each entry represents a paragraph)
   */
  ftxui::Element DrawSongLyrics(const lyric::SongLyric& lyrics);

  /* ******************************************************************************************** */
  //! Variables

  model::Song audio_info_;   //!< Audio information from current song
  lyric::SongLyric lyrics_;  //!< Song lyrics from current song
  int focused_ = 0;          //!< Index for paragraph focused from song lyric

  std::unique_ptr<lyric::LyricFinder> finder_ = lyric::LyricFinder::Create();  //!< Lyric finder
  std::future<FetchResult> async_fetcher_;  //!< Use lyric finder asynchronously

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::TabViewerTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_TAB_ITEM_SONG_LYRIC_H_
