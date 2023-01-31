/**
 * \file
 * \brief  Class for rendering a frequency bar for equalization
 */

#ifndef INCLUDE_VIEW_ELEMENT_FREQUENCY_BAR_H_
#define INCLUDE_VIEW_ELEMENT_FREQUENCY_BAR_H_

#include "ftxui/component/event.hpp"  // for Event
#include "ftxui/dom/elements.hpp"     // for Element
#include "model/audio_filter.h"

namespace interface {

/**
 * @brief Customized element to draw and control a frequency bar
 */
class FrequencyBar {
  static constexpr int kMaxGainLength = 8;  //!< Maximum rows for the Component

 public:
  /**
   * @brief Construct a new FrequencyBar object
   */
  explicit FrequencyBar(const model::AudioFilter& filter);

  /**
   * @brief Destroy FrequencyBar object
   */
  virtual ~FrequencyBar() = default;

  /* ******************************************************************************************** */
  //! Remove these
  FrequencyBar(const FrequencyBar& other) = delete;             // copy constructor
  FrequencyBar(FrequencyBar&& other) = delete;                  // move constructor
  FrequencyBar& operator=(const FrequencyBar& other) = delete;  // copy assignment
  FrequencyBar& operator=(FrequencyBar&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render();

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event);

  /**
   * @brief Get audio filter
   * @return Audio filter
   */
  model::AudioFilter GetAudioFilter() const;

  /* ******************************************************************************************** */
  //! Variables
 private:
  //! Style for frequency bar
  struct BarStyle {
    ftxui::Color background;
    ftxui::Color foreground;
  };

  BarStyle style_normal_, style_focused_;  //!< Color style
  ftxui::Box box_;                         //!< Box to control if mouse cursor is over the bar
  bool active_;                            //!< Flag to indicate if bar is active
  bool focused_;                           //!< Flag to indicate if bar is focused
  bool clicked_;                           //!< Flag to indicate if bar was clicked

  model::AudioFilter filter_bar_;  //!< Audio frequency gauge for equalization
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_FREQUENCY_BAR_H_