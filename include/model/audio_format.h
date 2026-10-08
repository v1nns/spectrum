/**
 * \file
 * \brief  Structure for format of audio samples
 */

#ifndef INCLUDE_MODEL_AUDIO_FORMAT_H_
#define INCLUDE_MODEL_AUDIO_FORMAT_H_

#include <cstdint>
#include <ostream>

namespace model {

/**
 * @brief Format of a single audio sample (always signed integer, with interleaved channels)
 */
enum class SampleFormat : uint8_t {
  S16,  //!< 16 bits
  S32,  //!< 32 bits
};

/**
 * @brief Format of audio samples sent from decoder to playback. By default, it is the one that
 * every output device is expected to play
 */
struct AudioFormat {
  uint32_t sample_rate = 44100;                    //!< Number of samples per second
  SampleFormat sample_format = SampleFormat::S16;  //!< Format of each sample
  uint16_t channels = 2;                           //!< Number of channels

  //! Number of bits used by each sample
  uint32_t GetBitDepth() const { return sample_format == SampleFormat::S16 ? 16 : 32; }

  //! Overloaded operators
  friend bool operator==(const AudioFormat& lhs, const AudioFormat& rhs) {
    return lhs.sample_rate == rhs.sample_rate && lhs.sample_format == rhs.sample_format &&
           lhs.channels == rhs.channels;
  }
  friend bool operator!=(const AudioFormat& lhs, const AudioFormat& rhs) { return !(lhs == rhs); }

  //! Output to ostream
  friend std::ostream& operator<<(std::ostream& out, const AudioFormat& format) {
    out << "{rate:" << format.sample_rate << " bits:" << format.GetBitDepth()
        << " channels:" << format.channels << "}";
    return out;
  }
};

}  // namespace model
#endif  // INCLUDE_MODEL_AUDIO_FORMAT_H_
