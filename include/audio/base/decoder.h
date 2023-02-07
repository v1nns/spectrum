/**
 * \file
 * \brief Interface class for decoder support
 */

#ifndef INCLUDE_AUDIO_BASE_DECODER_H_
#define INCLUDE_AUDIO_BASE_DECODER_H_

#include <functional>
#include <vector>

#include "model/application_error.h"
#include "model/audio_filter.h"
#include "model/song.h"
#include "model/volume.h"

namespace driver {

/**
 * @brief Common interface to read audio file as an input stream, decode it, apply biquad IIR
 * filters on extracted audio data and finally, send the result to audio callback
 */
class Decoder {
 public:
  /**
   * @brief Construct a new Decoder object
   */
  Decoder() = default;

  /**
   * @brief Destroy the Decoder object
   */
  virtual ~Decoder() = default;

  /* ******************************************************************************************** */
  //! Public API for Decoder

  /**
   * @brief Function invoked after resample is available.
   * (for better understanding: take a look at Audio Loop from Player, and also Playback class)
   */
  using AudioCallback = std::function<bool(void*, int, int64_t&)>;

  /**
   * @brief Open file as input stream and check for codec compatibility for decoding
   * @param audio_info (In/Out) In case of success, this is filled with detailed audio information
   * @return error::Code Application error code
   */
  virtual error::Code OpenFile(model::Song& audio_info) = 0;

  /**
   * @brief Decode and resample input stream to desired sample format/rate
   * @param samples Maximum value of samples
   * @param callback Pass resamples to this callback
   * @return error::Code Application error code
   */
  virtual error::Code Decode(int samples, AudioCallback callback) = 0;

  /**
   * @brief After file is opened and decoded, or when some error occurs, always clear internal cache
   */
  virtual void ClearCache() = 0;

  /* ******************************************************************************************** */
  //! Public API for Equalizer TODO: split into a new header along with FFmpeg class

  /**
   * @brief Set volume on playback stream
   *
   * @param value Desired volume (in a range between 0.f and 1.f)
   * @return error::Code Decoder error converted to application error code
   */
  virtual error::Code SetVolume(model::Volume value) = 0;

  /**
   * @brief Get volume from playback stream
   * @return model::Volume Volume percentage (in a range between 0.f and 1.f)
   */
  virtual model::Volume GetVolume() const = 0;

  /**
   * @brief Update audio filters in the filter chain (used for equalization)
   *
   * @param filters Audio filters
   * @return error::Code Decoder error converted to application error code
   */
  virtual error::Code UpdateFilters(const std::vector<model::AudioFilter>& filters) = 0;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_BASE_DECODER_H_
