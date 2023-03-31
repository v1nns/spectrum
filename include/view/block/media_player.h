/**
 * \file
 * \brief  Class for block containing audio player
 */

#ifndef INCLUDE_VIEW_BLOCK_AUDIO_PLAYER_H_
#define INCLUDE_VIEW_BLOCK_AUDIO_PLAYER_H_

#include <memory>  // for shared_ptr, unique_ptr

#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/dom/elements.hpp"              // for Element
#include "model/song.h"                        // for Song
#include "model/volume.h"
#include "view/base/block.h"  // for Block, BlockEvent (ptr...
#include "view/element/button.h"

namespace interface {

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
 */
class MediaPlayer : public Block {
  static constexpr int kMaxRows = 10;  //!< Maximum rows for the Component

 public:
  /**
   * @brief Construct a new Audio Player object
   * @param dispatcher Block event dispatcher
   */
  explicit MediaPlayer(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the Audio Player object
   */
  ~MediaPlayer() override = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event) override;

  /**
   * @brief Handles a custom event
   *
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
 private:
  //! Handle mouse event
  bool OnMouseEvent(ftxui::Event event) const;

  //! Utility to check media state
  bool IsPlaying() const {
    return song_.curr_info.state == model::Song::MediaState::Play ||
           song_.curr_info.state == model::Song::MediaState::Pause;
  }

  /* ******************************************************************************************** */
  //! Variables
  MediaButton btn_play_;              //!< Media player button for Play/Pause
  MediaButton btn_stop_;              //!< Media player button for Stop
  model::Song song_ = model::Song{};  //!< Audio information from current song
  model::Volume volume_;              //!< General sound volume

  ftxui::Box duration_box_;  //!< Box for duration component (gauge)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_AUDIO_PLAYER_H_
