/**
 * \file
 * \brief  Class for tab view containing spectrum visualizer
 */

#ifndef INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_VISUALIZER_H_
#define INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_VISUALIZER_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "ftxui/dom/canvas.hpp"
#include "model/bar_animation.h"
#include "util/file_handler.h"
#include "view/element/flash_message.h"
#include "view/element/tab.h"
#include "view/element/visualizer_picker.h"

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

  //! Effects are updated on each frame received from audio analysis (60 frames per second)
  static constexpr int kPeakHoldFrames = 20;        //!< Frames that peak cap stays before falling
  static constexpr double kPeakGravity = 0.0015;    //!< Speed gained by falling peak cap per frame
  static constexpr double kHueStep = 0.5;           //!< Degrees that hue rotates per frame
  static constexpr int kWaterfallFrameStep = 4;     //!< Frames for each row added to waterfall
  static constexpr size_t kWaterfallMaxRows = 200;  //!< Rows kept by waterfall

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
   * @param file_handler Utility handler to load/save visualizer settings
   */
  explicit SpectrumVisualizer(const model::BlockIdentifier& id,
                              const std::shared_ptr<EventDispatcher>& dispatcher,
                              const FocusCallback& on_focus, const keybinding::Key& keybinding,
                              const std::shared_ptr<util::FileHandler>& file_handler);

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
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event& event) override;

  /**
   * @brief Close animation picker when another tab item is shown instead of this one (otherwise,
   * it would still be open when this one is shown again)
   */
  void OnLostFocus() override;

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
   * @brief Set if visualizer is dimmed when rendered (e.g. while a dialog is opened over it)
   * @param dimmed True if dim is applied over visualizer
   */
  void SetDimmed(bool dimmed) { dimmed_ = dimmed; }

  /**
   * @brief Get current bar animation (used for Terminal calculation)
   * @return Bar animation identifier
   */
  model::BarAnimation GetAnimation() const { return options_.animation; }

  /**
   * @brief Check if there is an empty column between bars (used for Terminal calculation)
   * @return true if bars are spaced, otherwise false
   */
  bool IsBarSpaced() const { return model::IsBarAnimation(options_.animation) && options_.spacing; }

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
  //! Colors for a single frame (from theme, with hue cycle applied), defined in source file
  struct Colorizer;

  //! Cap on recent maximum from a bar
  struct Peak {
    double value = 0;   //!< Bar value where cap is
    int hold = 0;       //!< Frames left before it starts falling
    double speed = 0;   //!< Falling speed (per frame)
  };

  //! Change current options and notify terminal when number of bars may change
  void SetOptions(const model::VisualizerOptions& options);

  //! Save current options (unless they are being previewed by picker) and bar width, so they are
  //! restored on next run
  void SaveSettings() const;

  //! Update effects (peak caps, hue cycle and waterfall) with the frame just received
  void UpdateEffects();

  //! Get colors for the frame being rendered
  Colorizer GetColorizer() const;

  //! Get peak cap for the bar with the given index (or nothing when peak caps are disabled)
  std::optional<double> GetPeak(size_t index) const;

  /**
   * @brief Create bar with the current style, appending its columns (and space, if any)
   * @param value Bar value, from 0.0 to 1.0
   * @param peak Peak cap value (if any)
   * @param column Bar position, from the first bar (0.0) to the last one (1.0)
   * @param direction Direction where bar is filled
   * @param colorizer Colors for current frame
   * @param elements Elements to append bar
   */
  void CreateGauge(double value, std::optional<double> peak, double column,
                   ftxui::Direction direction, const Colorizer& colorizer,
                   ftxui::Elements& elements) const;

  //! Animations
  void DrawAnimationHorizontalMirror(ftxui::Element& visualizer, const Colorizer& colorizer);
  void DrawAnimationVerticalMirror(ftxui::Element& visualizer, const Colorizer& colorizer);
  void DrawAnimationMono(ftxui::Element& visualizer, const Colorizer& colorizer);
  void DrawAnimationLine(ftxui::Element& visualizer, LineStyle style, const Colorizer& colorizer);
  void DrawAnimationRadial(ftxui::Element& visualizer, const Colorizer& colorizer);
  void DrawAnimationWaterfall(ftxui::Element& visualizer, const Colorizer& colorizer);

  //! Helpers to draw spectrum as a line on canvas (for each LineStyle)
  //! Vertical run of dots in a canvas column
  struct VerticalRun {
    int x;         //!< Column
    int from;      //!< First row
    int to;        //!< Last row (may be lower than first row)
    double level;  //!< Spectrum value in this column, from 0.0 to 1.0
  };

  //! Reference used to color dots based on their distance to it
  struct Baseline {
    int y;      //!< Row where gradient starts
    int range;  //!< Distance from baseline where gradient ends
  };

  static void DrawRun(ftxui::Canvas& canvas, const VerticalRun& run, const Baseline& baseline,
                      const Colorizer& colorizer);
  static void DrawLine(ftxui::Canvas& canvas, const std::vector<double>& data,
                       const Colorizer& colorizer);
  static void DrawMirroredLines(ftxui::Canvas& canvas, const std::vector<double>& left,
                                const std::vector<double>& right, const Colorizer& colorizer);
  static void DrawFilledArea(ftxui::Canvas& canvas, const std::vector<double>& data,
                             const Colorizer& colorizer);
  static void DrawMirroredFilledAreas(ftxui::Canvas& canvas, const std::vector<double>& left,
                                      const std::vector<double>& right,
                                      const Colorizer& colorizer);
  static void FillBlocks(ftxui::Canvas& canvas, const VerticalRun& run, const Baseline& baseline,
                         const Colorizer& colorizer);
  static void DrawRays(ftxui::Canvas& canvas, const std::vector<double>& left,
                       const std::vector<double>& right, const Colorizer& colorizer);

  /* ******************************************************************************************** */
  //! Variables
  model::VisualizerOptions options_;   //!< Control which animation to draw and how it looks
  std::vector<double> spectrum_data_;  //!< Audio spectrum (each entry represents a frequency bar)
  int gauge_width_ = kGaugeDefaultWidth;  //!< Current audio bar width
  bool dimmed_ = false;                   //!< Dim is applied over visualizer when rendered

  std::vector<Peak> peaks_;  //!< Peak cap for each bar from spectrum data

  double hue_ = 0;             //!< Rotation applied to gradient colors (in degrees)
  double flash_ = 0;           //!< Brightness added to colors by the last beat (from 0.0 to 1.0)
  double bass_average_ = 0;    //!< Recent average from lowest frequencies (to detect beats)
  int frames_since_beat_ = 0;  //!< Frames received since the last beat

  //! Spectrum from each moment (average from both channels), from the newest to the oldest one
  std::deque<std::vector<double>> waterfall_;
  int waterfall_frames_ = 0;  //!< Frames received since the last row was added to waterfall

  FlashMessage message_;  //!< Brief feedback shown over visualizer (e.g. animation name)

  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save visualizer settings

  //! Options picker (shown over visualizer, changing it while options change)
  VisualizerPicker picker_;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_MAIN_CONTENT_AUDIO_VISUALIZER_H_
