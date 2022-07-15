/**
 * \file
 * \brief  Class for block containing audio visualizer
 */

#ifndef INCLUDE_VIEW_BLOCK_AUDIO_VISUALIZER_H_
#define INCLUDE_VIEW_BLOCK_AUDIO_VISUALIZER_H_

#include <memory>

#include "view/base/block.h"

namespace interface {

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
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

  /* ******************************************************************************************* */
  //! Variables
 private:
};

}  // namespace interface

#endif  // INCLUDE_VIEW_BLOCK_AUDIO_VISUALIZER_H_
