/**
 * \file
 * \brief  Repeat mode used to play songs from queue
 */

#ifndef INCLUDE_MODEL_REPEAT_MODE_H_
#define INCLUDE_MODEL_REPEAT_MODE_H_

#include <cstdint>
#include <ostream>
#include <string_view>

namespace model {

/**
 * @brief Repeat mode for songs from queue (playlist or files from directory)
 */
enum class RepeatMode : std::uint8_t {
  Off,  //!< Stop after last song
  All,  //!< Play first song again after last one
  One,  //!< Play current song again
};

//! Get next repeat mode (cycling through all of them)
inline RepeatMode GetNextRepeatMode(RepeatMode mode) {
  switch (mode) {
    case RepeatMode::Off:
      return RepeatMode::All;
    case RepeatMode::All:
      return RepeatMode::One;
    case RepeatMode::One:
      break;
  }

  return RepeatMode::Off;
}

//! Get repeat mode name
inline std::string_view GetRepeatModeName(RepeatMode mode) {
  switch (mode) {
    case RepeatMode::All:
      return "all";
    case RepeatMode::One:
      return "one";
    case RepeatMode::Off:
      break;
  }

  return "off";
}

//! Output repeat mode to ostream
inline std::ostream& operator<<(std::ostream& out, RepeatMode mode) {
  return out << GetRepeatModeName(mode);
}

}  // namespace model
#endif  // INCLUDE_MODEL_REPEAT_MODE_H_
