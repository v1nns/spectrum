/**
 * \file
 * \brief  Structure for audio output in use
 */

#ifndef INCLUDE_MODEL_AUDIO_OUTPUT_H_
#define INCLUDE_MODEL_AUDIO_OUTPUT_H_

#include <ostream>
#include <string>

#include "model/audio_format.h"

namespace model {

/**
 * @brief Where current song is being played, and how: output device in use and format of audio
 * samples sent to it (which may not be the same one from song, as it depends on what is supported
 * by device)
 */
struct AudioOutput {
  std::string device;  //!< Name of output device
  AudioFormat format;  //!< Format of audio samples sent to output device

  //! Overloaded operators
  friend bool operator==(const AudioOutput& lhs, const AudioOutput& rhs) {
    return lhs.device == rhs.device && lhs.format == rhs.format;
  }
  friend bool operator!=(const AudioOutput& lhs, const AudioOutput& rhs) { return !(lhs == rhs); }

  //! Output to ostream
  friend std::ostream& operator<<(std::ostream& out, const AudioOutput& output) {
    out << "{device:" << output.device << " format:" << output.format << "}";
    return out;
  }
};

}  // namespace model
#endif  // INCLUDE_MODEL_AUDIO_OUTPUT_H_
