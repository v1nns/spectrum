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
#include "view/base/block.h"                   // for Block, BlockEvent (ptr...
#include "view/element/button.h"

namespace interface {

using MediaButton = std::shared_ptr<Button>;

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
 */
class MediaPlayer : public Block {
 public:
  /**
   * @brief Construct a new Audio Player object
   * @param dispatcher Block event dispatcher
   */
  explicit MediaPlayer(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the Audio Player object
   */
  virtual ~MediaPlayer() = default;

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
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
 private:
  //! Handle mouse event
  bool OnMouseEvent(ftxui::Event event);

  /* ******************************************************************************************** */
 private:
  MediaButton btn_play_, btn_stop_;  //!< Media player buttons
  model::Song audio_info_;           //!< Audio information from current song
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_AUDIO_PLAYER_H_