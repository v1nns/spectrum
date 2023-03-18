/**
 * \file
 * \brief  Class for tab view containing audio equalizer control
 */

#ifndef INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_
#define INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_

#include "model/audio_filter.h"
#include "view/element/button.h"
#include "view/element/frequency_bar.h"
#include "view/element/tab_item.h"

namespace interface {

/**
 * @brief Component to control multiple frequency bars, in order to setup audio equalization
 */
class AudioEqualizer : public TabItem {
  static constexpr int kInvalidIndex = -1;  //!< Invalid index (to use it when no bar is focused)

 public:
  /**
   * @brief Construct a new AudioEqualizer object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   */
  explicit AudioEqualizer(const model::BlockIdentifier& id,
                          const std::shared_ptr<EventDispatcher>& dispatcher);

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
  //! Private methods
 private:
  //! Update UI components state based on internal cache
  void UpdateInterfaceState();

  //! Update focus state in both old and new frequency bar focused
  void UpdateFocus(int old_index);

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::vector<model::AudioFilter> cache_;            //!< Last applied filters
  std::vector<std::unique_ptr<FrequencyBar>> bars_;  //!< Audio frequency bars
  GenericButton btn_apply_, btn_reset_;              //!< Buttons to setup equalization

  int focused_;  //!< Index to current bar focused
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_