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
  static constexpr int kMaxGainLength = 8;  //!< Maximum string length in the input box for gain

 public:
  /**
   * @brief Construct a new FrequencyBar object
   */
  explicit FrequencyBar(const model::AudioFilter& filter);

  /**
   * @brief Destroy FrequencyBar object
   */
  ~FrequencyBar() = default;

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

  /**
   * @brief Set frequency bar state as focused
   */
  void SetFocus();

  /**
   * @brief Increment gain value by 1 dB
   */
  void IncreaseGain();

  /**
   * @brief Decrement gain value by 1 dB
   */
  void DecreaseGain();

  /**
   * @brief Reset gain to default value (zero)
   */
  void ResetGain();

  /**
   * @brief Reset focus to default value (not focused)
   */
  void ResetFocus();

  /* ******************************************************************************************** */
  //! Variables
 private:
  //! Style for frequency bar
  struct BarStyle {
    ftxui::Color background;
    ftxui::Color foreground;
  };

  //!< Color styles
  BarStyle style_normal_ = BarStyle{.background = ftxui::Color::LightSteelBlue3,
                                    .foreground = ftxui::Color::SteelBlue3};  //!< Normal mode

  BarStyle style_hovered_ = BarStyle{.background = ftxui::Color::LightSteelBlue1,
                                     .foreground = ftxui::Color::SlateBlue1};  //!< On hover state

  BarStyle style_focused_ = BarStyle{.background = ftxui::Color::LightSteelBlue3,
                                     .foreground = ftxui::Color::RedLight};  //!< On focus state

  ftxui::Box box_;        //!< Box to control if mouse cursor is over the bar
  bool hovered_ = false;  //!< Flag to indicate if bar is hovered (by mouse)
  bool focused_ = false;  //!< Flag to indicate if bar is focused

  model::AudioFilter filter_bar_;  //!< Audio frequency gauge for equalization
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_FREQUENCY_BAR_H_