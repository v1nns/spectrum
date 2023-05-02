/**
 * \file
 * \brief  Class for tab  iew containing song lyrics
 */

#ifndef INCLUDE_VIEW_BLOCK_TAB_ITEM_SONG_LYRIC_H_
#define INCLUDE_VIEW_BLOCK_TAB_ITEM_SONG_LYRIC_H_

#include <atomic>
#include <mutex>
#include <optional>

#include "audio/lyric/lyric_finder.h"
#include "model/song.h"
#include "view/element/tab_item.h"

namespace interface {

// To better readability
using LyricFinder = std::unique_ptr<lyric::LyricFinder>;

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
  //! TODO: doc
  void FetchSong();

  /* ******************************************************************************************** */
  //! Variables
  model::Song audio_info_;  //!< Audio information from current song

  LyricFinder finder_ = lyric::LyricFinder::Create();  //!< Lyric finder
  std::optional<lyric::SongLyric> lyrics_;             //!< Song lyrics from current song

  std::mutex mutex_;            //!< Control access for internal resources
  std::atomic<bool> fetching_;  //!< Notify if fetch operation is being executed
  std::atomic<bool> failed_;    //!< Notify if fetch operation failed
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_TAB_ITEM_SONG_LYRIC_H_