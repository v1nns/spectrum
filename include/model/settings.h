/**
 * \file
 * \brief  Settings saved between runs
 */

#ifndef INCLUDE_MODEL_SETTINGS_H_
#define INCLUDE_MODEL_SETTINGS_H_

#include <optional>
#include <string>
#include <vector>

#include "model/bar_animation.h"
#include "model/repeat_mode.h"

namespace model {

/**
 * @brief User settings restored at startup (any of them may be missing from settings file)
 */
struct Settings {
  std::optional<BarAnimation> animation;  //!< Spectrum visualizer animation
  std::optional<int> bar_width;           //!< Spectrum visualizer bar width
  std::optional<int> volume;              //!< Player volume (percentage, from 0 to 100)
  std::optional<std::string> device;      //!< Audio output device (empty to not choose any)
  std::optional<RepeatMode> repeat;       //!< Repeat mode for songs from queue
  std::optional<bool> shuffle;            //!< Shuffle songs from queue
  std::optional<std::string> theme;       //!< Identifier from UI theme

  std::optional<std::string> equalizer_preset;  //!< Name of preset applied by equalizer

  //! Gain (in dB) for each frequency from the equalizer preset that may be modified by user
  std::optional<std::vector<double>> equalizer_custom;

  //! Browser whose cookies are sent to site when it refuses to stream songs (as expected by yt-dlp)
  std::optional<std::string> cookies_from_browser;
};

}  // namespace model
#endif  // INCLUDE_MODEL_SETTINGS_H_
