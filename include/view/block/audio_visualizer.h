/**
 * \file
 * \brief  Class for block containing audio visualizer
 */

#ifndef INCLUDE_VIEW_BLOCK_AUDIO_VISUALIZER_H_
#define INCLUDE_VIEW_BLOCK_AUDIO_VISUALIZER_H_

#include <memory>

#include "view/base/block.h"
#include "view/element/button.h"

namespace interface {

using WindowButton = std::shared_ptr<Button>;

/**
 * @brief Component with audio spectrum in realtime from current music playing
 */
class AudioVisualizer : public Block {
 public:
  /**
   * @brief Construct a new Audio Visualizer object
   * @param dispatcher Block event dispatcher
   */
  explicit AudioVisualizer(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the File Info object
   */
  virtual ~AudioVisualizer() = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
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

  /**
   * @brief Possible bar animations for spectrum visualizer
   */
  enum Animation {
    HorizontalMirror,  //!< Both channels (L/R) are mirrored horizontally (default)
    VerticalMirror,    //!< Both channels (L/R) are mirrored vertically
    LAST,
  };

  /* ******************************************************************************************** */
  //! Private methods
 private:
  //! Handle mouse event
  bool OnMouseEvent(ftxui::Event event);

  //! Animations
  void DrawAnimationHorizontalMirror(ftxui::Element& visualizer);
  void DrawAnimationVerticalMirror(ftxui::Element& visualizer);

  /* ******************************************************************************************** */
  //! Variables
 private:
  WindowButton btn_help_, btn_exit_;  //!< Buttons located on the upper-right border of block window
  Animation curr_anim_;               //!< Flag to control which animation to draw
  std::vector<double> data_;  //!< Audio spectrum for stereo (each entry represents a frequency bar)
};

}  // namespace interface

#endif  // INCLUDE_VIEW_BLOCK_AUDIO_VISUALIZER_H_
