/**
 * \file
 * \brief  Class for tab view containing audio equalizer control
 */

#ifndef INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_
#define INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_

#include "ftxui/component/component.hpp"
#include "model/audio_filter.h"
#include "view/element/button.h"
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
  ~AudioEqualizer() override = default;

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
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(const ftxui::Event& event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
  //! Private methods
 private:
  //! Handle mapped keyboard events for navigation
  bool OnNavigationEvent(const ftxui::Event& event);

  //! Update UI components state based on internal cache
  void UpdateButtonState();

  //! Update focus state in both old and new frequency bar focused
  void UpdateFocus(int old_index, int new_index);

  /* ******************************************************************************************** */
  //! Internal structures

  struct FrequencyBar {
    //! Style for frequency bar
    struct BarStyle {
      ftxui::Color background;
      ftxui::Color foreground;
    };

    //!< Color styles
    BarStyle style_normal = BarStyle{.background = ftxui::Color::LightSteelBlue3,
                                     .foreground = ftxui::Color::SteelBlue3};  //!< Normal mode

    BarStyle style_hovered = BarStyle{.background = ftxui::Color::LightSteelBlue1,
                                      .foreground = ftxui::Color::SlateBlue1};  //!< On hover state

    BarStyle style_focused = BarStyle{.background = ftxui::Color::LightSteelBlue3,
                                      .foreground = ftxui::Color::RedLight};  //!< On focus state

    ftxui::Box box;              //!< Box to control if mouse cursor is over the bar
    bool hovered = false;        //!< Flag to indicate if bar is hovered (by mouse)
    bool focused = false;        //!< Flag to indicate if bar is focused (set by equalizer)
    model::AudioFilter* filter;  //!< Audio frequency filters for equalization

    /**
     * @brief Set element as focused
     */
    void SetFocus() { focused = true; }

    /**
     * @brief Reset focus from element
     */
    void ResetFocus() { focused = false; }

    /**
     * @brief Render frequency bar
     * @return UI element
     */
    ftxui::Element Render() {
      using ftxui::EQUAL;
      using ftxui::WIDTH;
      static constexpr int kMaxGainLength = 8;  //!< Maximum string length in the input box for gain

      constexpr auto empty_line = []() { return ftxui::text(""); };

      constexpr auto gen_slider = [&](float value, const BarStyle& style) {
        ftxui::Decorator color = ftxui::bgcolor(style.background) | ftxui::color(style.foreground);

        return ftxui::hbox({
            ftxui::gaugeUp(value) | ftxui::yflex_grow | color,
            ftxui::gaugeUp(value) | ftxui::yflex_grow | color,
        });
      };

      // Get gain value and choose style
      float gain = filter->GetGainAsPercentage();
      const BarStyle& style = focused ? style_focused : hovered ? style_hovered : style_normal;

      return ftxui::vbox({
          // title
          empty_line(),
          ftxui::text(filter->GetFrequency()) | ftxui::hcenter,
          empty_line(),

          // frequency gauge
          gen_slider(gain, style) | ftxui::hcenter | ftxui::yflex_grow | ftxui::reflect(box),

          // gain input
          empty_line(),
          ftxui::text(filter->GetGain()) | ftxui::inverted | ftxui::hcenter |
              ftxui::size(WIDTH, EQUAL, kMaxGainLength),
          empty_line(),
      });
    }

    /**
     * @brief Handles an event (from mouse)
     * @param event Received event from screen
     * @return true if event was handled, otherwise false
     */
    bool OnMouseEvent(ftxui::Event event) {
      if (event.mouse().button == ftxui::Mouse::WheelDown ||
          event.mouse().button == ftxui::Mouse::WheelUp) {
        if (hovered) {
          double increment = event.mouse().button == ftxui::Mouse::WheelUp ? 1 : -1;
          filter->SetNormalizedGain(filter->gain + increment);

          return true;
        }

        return false;
      }

      if (box.Contain(event.mouse().x, event.mouse().y)) {
        hovered = true;

        if (event.mouse().button == ftxui::Mouse::Left &&
            event.mouse().motion == ftxui::Mouse::Released) {
          // Calculate new value for gain based on coordinates from mouse click and bar size
          double value =
              std::ceil(model::AudioFilter::kMaxGain -
                        (event.mouse().y - box.y_min) *
                            (model::AudioFilter::kMaxGain - model::AudioFilter::kMinGain) /
                            (box.y_max - box.y_min));

          filter->SetNormalizedGain(value);
          return true;
        }
      } else {
        hovered = false;
      }

      return false;
    }
  };

  //   struct GenrePicker {};

  /* ******************************************************************************************** */
  //! Variables

  //! Equalizer settings
  model::EqualizerPreset eq_last_applied_ =
      model::AudioFilter::CreateCustomPreset();  //!< Last EQ settings applied

  model::EqualizerPreset eq_custom_ =
      model::AudioFilter::CreateCustomPreset();  //!< Custom EQ settings (created manually by user)

  //   model::EqualizerPresets eq_presets_ =
  //       model::AudioFilter::CreateGenrePresets();  //!< EQ settings for different music genres

  /* ******************************************************************************************** */
  //! Interface elements

  using FrequencyBars = std::array<FrequencyBar, model::AudioFilter::kPresetSize>;
  FrequencyBars bars_;  //!< Array of gauges for EQ settings

  GenericButton btn_apply_;  //!< Buttons to apply equalization
  GenericButton btn_reset_;  //!< Buttons to reset equalization

  /* ******************************************************************************************** */
  //! Internal focus handling

  int elem_focused_ = kInvalidIndex;  //!< Index to current element focused
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_TAB_ITEM_AUDIO_EQUALIZER_H_