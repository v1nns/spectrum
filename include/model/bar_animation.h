/**
 * \file
 * \brief Structure for a bar animation and options to customize how spectrum visualizer draws it
 */

#ifndef INCLUDE_MODEL_BAR_ANIMATION_H_
#define INCLUDE_MODEL_BAR_ANIMATION_H_

#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

namespace model {

/**
 * @brief Contains bar animations that can be rendered by spectrum visualizer
 */
enum BarAnimation {
  HorizontalMirror = 11000,  //!< Both channels (L/R) are mirrored horizontally (default)
  VerticalMirror,            //!< Both channels (L/R) are mirrored vertically
  Mono,                      //!< Average from the sum of both channels (L/R)
  SpectrumLine,              //!< Line connecting the average of both channels (L/R)
  SpectrumLineMirror,        //!< Lines from both channels (L/R) mirrored vertically
  SpectrumLineFilled,        //!< Same as SpectrumLine, but filling the area below the line
  SpectrumLineFilledMirror,  //!< Same as SpectrumLineMirror, but filling the area of lines
  Radial,                    //!< Bars pointing out from a circle (left channel on its left half)
  Waterfall,                 //!< Spectrum from each moment as a row, scrolling down over time
  LAST,
};

//! BarAnimation pretty print
std::ostream& operator<<(std::ostream& out, const BarAnimation& animation);

/**
 * @brief Get user-friendly name for animation (to be shown on UI)
 * @param animation Bar animation
 * @return Animation name
 */
std::string_view GetAnimationName(const BarAnimation& animation);

/**
 * @brief Get identifier for animation (to be saved in settings, as it never changes)
 * @param animation Bar animation
 * @return Animation identifier (e.g. "line-filled")
 */
std::string_view GetAnimationId(const BarAnimation& animation);

/**
 * @brief Get animation from its identifier
 * @param id Animation identifier
 * @return Bar animation, or nothing when identifier is unknown
 */
std::optional<BarAnimation> GetAnimationFromId(std::string_view id);

/**
 * @brief Check if animation draws bars (which can be customized by bar style, spacing and peaks)
 * @param animation Bar animation
 * @return true if animation draws bars, otherwise false
 */
inline bool IsBarAnimation(const BarAnimation& animation) {
  return animation == HorizontalMirror || animation == VerticalMirror || animation == Mono;
}

/**
 * @brief Check if animation draws each channel (or their average) across the whole width, instead
 * of splitting width between both channels. In this case, it needs twice the number of bars that
 * fit on screen from audio analysis
 * @param animation Bar animation
 * @return true if animation uses the whole width for each channel, otherwise false
 */
inline bool IsAnimationFullWidthPerChannel(const BarAnimation& animation) {
  return animation != HorizontalMirror && animation != LAST;
}

/* ********************************************************************************************** */

//! How each bar is drawn
enum class BarStyle : uint8_t {
  Solid,    //!< Blocks filled in eighths of a cell (default)
  Led,      //!< Separate segments, like a hardware equalizer
  Braille,  //!< Braille dots
  LAST,
};

//! How colors from theme gradient are spread over the spectrum
enum class BarColors : uint8_t {
  Vertical,        //!< Gradient from base of bars to the top of the area (default)
  Level,           //!< Whole bar takes a single color, picked by its height
  LevelStretched,  //!< Gradient squeezed into each bar, so all of them end in the last color
  Horizontal,      //!< Gradient across bars, from the lowest frequency to the highest
  Diagonal,        //!< Gradient from the base of the first bar to the top of the last one
  Zones,           //!< Green, yellow and red zones (fixed by height, like a hardware equalizer)
  LAST,
};

//! Rotation of gradient colors over time
enum class HueCycle : uint8_t {
  Off,    //!< Colors do not change (default)
  On,     //!< Colors slowly rotate their hue
  Flash,  //!< Same as On, and colors flash brighter on each beat
  LAST,
};

std::string_view GetName(BarStyle style);
std::string_view GetName(BarColors colors);
std::string_view GetName(HueCycle hue);

//! Identifiers saved in settings (they never change)
std::string_view GetId(BarStyle style);
std::string_view GetId(BarColors colors);
std::string_view GetId(HueCycle hue);

//! Get option from its identifier (or nothing when identifier is unknown)
std::optional<BarStyle> GetBarStyleFromId(std::string_view id);
std::optional<BarColors> GetBarColorsFromId(std::string_view id);
std::optional<HueCycle> GetHueCycleFromId(std::string_view id);

/**
 * @brief Everything that user can choose about how spectrum visualizer looks
 */
struct VisualizerOptions {
  BarAnimation animation = BarAnimation::HorizontalMirror;  //!< What is drawn
  BarStyle bars = BarStyle::Solid;                          //!< How each bar is drawn
  bool spacing = true;                                      //!< Empty column between bars
  BarColors colors = BarColors::Vertical;                   //!< How gradient is spread
  bool peaks = false;               //!< Cap on recent maximum from each bar, falling over time
  HueCycle hue = HueCycle::Off;     //!< Rotation of colors over time

  bool operator==(const VisualizerOptions& other) const {
    return animation == other.animation && bars == other.bars && spacing == other.spacing &&
           colors == other.colors && peaks == other.peaks && hue == other.hue;
  }

  bool operator!=(const VisualizerOptions& other) const { return !(*this == other); }
};

}  // namespace model

#endif  // INCLUDE_MODEL_BAR_ANIMATION_H_
