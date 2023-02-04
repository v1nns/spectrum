/**
 * \file
 * \brief  Class for tab view containing audio equalizer control
 */

#ifndef INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_
#define INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_

#include "view/element/button.h"
#include "view/element/frequency_bar.h"
#include "view/element/tab_item.h"

namespace interface {

/**
 * @brief Component to control multiple frequency bars, in order to setup audio equalization
 */
class AudioEqualizer : public TabItem {
 public:
  /**
   * @brief Construct a new AudioEqualizer object
   * @param dispatcher Block event dispatcher
   */
  explicit AudioEqualizer(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the AudioEqualizer object
   */
  virtual ~AudioEqualizer() = default;

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
  bool OnEvent(ftxui::Event event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::vector<std::unique_ptr<FrequencyBar>> bars_;  //!< Audio frequency bars
  GenericButton btn_apply_, btn_reset_;              //!< Buttons to setup equalization
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_