/**
 * \file
 * \brief  Settings saved between runs
 */

#ifndef INCLUDE_MODEL_SETTINGS_H_
#define INCLUDE_MODEL_SETTINGS_H_

#include <optional>

#include "model/bar_animation.h"

namespace model {

/**
 * @brief User settings restored at startup (any of them may be missing from settings file)
 */
struct Settings {
  std::optional<BarAnimation> animation;  //!< Spectrum visualizer animation
  std::optional<int> bar_width;           //!< Spectrum visualizer bar width
  std::optional<int> volume;              //!< Player volume (percentage, from 0 to 100)
};

}  // namespace model
#endif  // INCLUDE_MODEL_SETTINGS_H_
