/**
 * \file
 * \brief  Structure for audio output device
 */

#ifndef INCLUDE_MODEL_AUDIO_DEVICE_H_
#define INCLUDE_MODEL_AUDIO_DEVICE_H_

#include <string>
#include <vector>

namespace model {

/**
 * @brief Audio output device available to play songs
 */
struct AudioDevice {
  std::string name;         //!< Identifier used to open device (e.g. "default")
  std::string description;  //!< Text to describe device to user (it may be empty)

  //! Overloaded operators
  friend bool operator==(const AudioDevice& lhs, const AudioDevice& rhs) {
    return lhs.name == rhs.name && lhs.description == rhs.description;
  }
  friend bool operator!=(const AudioDevice& lhs, const AudioDevice& rhs) { return !(lhs == rhs); }
};

using AudioDevices = std::vector<AudioDevice>;

}  // namespace model
#endif  // INCLUDE_MODEL_AUDIO_DEVICE_H_
