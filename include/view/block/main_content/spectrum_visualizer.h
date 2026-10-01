/**
 * \file
 * \brief  Class for tab view containing spectrum visualizer
 */

#ifndef INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_VISUALIZER_H_
#define INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_VISUALIZER_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ftxui/dom/canvas.hpp"
#include "model/bar_animation.h"
#include "view/element/flash_message.h"
#include "view/element/tab.h"

namespace interface {

/**
 * @brief Component to render different animations using audio spectrum data from current song
 */
class SpectrumVisualizer : public TabItem {
  static constexpr std::string_view kTabName = "visualizer";  //!< Tab title
  static constexpr int kGaugeDefaultWidth = 2;  //!< Default gauge width (audio bar width)
  static constexpr int kGaugeMinWidth = 1;      //!< Maximum value for gauge width
  static constexpr int kGaugeMaxWidth = 4;      //!< Minimum value for gauge width
  static constexpr int kGaugeSpacing = 1;       //!< Spacing between gauges

  static constexpr std::chrono::milliseconds kMessageDuration{2000};  //!< Time to show message

  //! Color stop from gradient used to draw spectrum (position goes from 0.0 to 1.0)
  struct ColorStop {
    float position;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };

  //! Gradient from the lowest to the highest part of spectrum
  static constexpr std::array<ColorStop, 4> kGradient{{
      {0.0F, 95, 135, 215},
      {0.3F, 115, 155, 215},
      {0.6F, 155, 188, 235},
      {0.8F, 185, 208, 252},
  }};

  //! Possible styles for animations drawing spectrum as a line
  enum class LineStyle : uint8_t {
    Plain,         //!< Single line (average from both channels)
    Mirror,        //!< Left channel above the middle and right channel below it
    Filled,        //!< Single line (average from both channels) with area below it filled
    FilledMirror,  //!< Same as Mirror, but with area between middle and each line filled
  };

 public:
  /**
   * @brief Construct a new SpectrumVisualizer object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   * @param on_focus Callback function to ask for focus
   * @param keybinding Keybinding to set item as active
   */
  explicit SpectrumVisualizer(const model::BlockIdentifier& id,
                              const std::shared_ptr<EventDispatcher>& dispatcher,
                              const FocusCallback& on_focus, const keybinding::Key& keybinding);

  /**
   * @brief Destroy the SpectrumVisualizer object
   */
  ~SpectrumVisualizer() override = default;

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

  /**
   * @brief Get width for a single bar (used for Terminal calculation)
   * @return Audio bar width
   */
  int GetBarWidth() const { return gauge_width_; }

  /**
   * @brief Briefly show a hint on how to exit fullscreen mode (along with current animation name)
   */
  void ShowFullscreenHint();

  /**
   * @brief Hide any message being shown on visualizer
   */
  void HideMessage();

  /* ******************************************************************************************** */
  // Private methods
 private:
  //! Utility to create UI gauge
  void CreateGauge(double value, ftxui::Direction direction, ftxui::Elements& elements,
                   bool space = true) const;

  //! Animations
  void DrawAnimationHorizontalMirror(ftxui::Element& visualizer, bool space = true);
  void DrawAnimationVerticalMirror(ftxui::Element& visualizer, bool space = true);
  void DrawAnimationMono(ftxui::Element& visualizer, bool space = true);
  void DrawAnimationLine(ftxui::Element& visualizer, LineStyle style);

  //! Helpers to draw spectrum as a line on canvas (for each LineStyle)
  //! Vertical run of dots in a canvas column
  struct VerticalRun {
    int x;     //!< Column
    int from;  //!< First row
    int to;    //!< Last row (may be lower than first row)
  };

  //! Reference used to color dots based on their distance to it
  struct Baseline {
    int y;      //!< Row where gradient starts
    int range;  //!< Distance from baseline where gradient ends
  };

  static void DrawRun(ftxui::Canvas& canvas, const VerticalRun& run, const Baseline& baseline);
  static void DrawLine(ftxui::Canvas& canvas, const std::vector<double>& data);
  static void DrawMirroredLines(ftxui::Canvas& canvas, const std::vector<double>& left,
                                const std::vector<double>& right);
  static void DrawFilledArea(ftxui::Canvas& canvas, const std::vector<double>& data);
  static void DrawMirroredFilledAreas(ftxui::Canvas& canvas, const std::vector<double>& left,
                                      const std::vector<double>& right);
  static void FillBlocks(ftxui::Canvas& canvas, const VerticalRun& run, const Baseline& baseline);

  //! Get color from spectrum gradient
  //! @param position Position in gradient, from 0.0 (lowest) to 1.0 (highest)
  static ftxui::Color GetGradientColor(double position);

  /* ******************************************************************************************** */
  //! Variables
  model::BarAnimation curr_anim_ =
      model::BarAnimation::HorizontalMirror;  //!< Control which bar animation to draw
  std::vector<double> spectrum_data_;  //!< Audio spectrum (each entry represents a frequency bar)
  int gauge_width_ = kGaugeDefaultWidth;  //!< Current audio bar width

  FlashMessage message_;  //!< Brief feedback shown over visualizer (e.g. animation name)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_VISUALIZER_H_
