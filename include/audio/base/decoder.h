/**
 * \file
 * \brief Interface class for decoder support
 */

#ifndef INCLUDE_AUDIO_BASE_DECODER_H_
#define INCLUDE_AUDIO_BASE_DECODER_H_

#include <functional>

#include "model/application_error.h"
#include "model/song.h"

namespace driver {

/**
 * @brief Common interface to read audio file as an input stream and parse its samples
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
  //! Public API

  /**
   * @brief Function invoked after resample is available.
   * (for better understanding: take a look at Audio Loop from Player, and also Playback class)
   */
  using AudioCallback = std::function<bool(void*, int, int, int)>;

  /**
   * @brief Open file as input stream and check for codec compatibility for decoding
   * @param audio_info (In/Out) In case of success, this is filled with detailed audio information
   * @return error::Code Application error code
   */
  virtual error::Code OpenFile(model::Song* audio_info) = 0;

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
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_BASE_DECODER_H_
