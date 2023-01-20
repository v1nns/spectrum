/**
 * \file
 * \brief Interface class for equalizer support
 */

#ifndef INCLUDE_AUDIO_BASE_EQUALIZER_H_
#define INCLUDE_AUDIO_BASE_EQUALIZER_H_

#include <functional>

#include "model/application_error.h"
#include "model/volume.h"

namespace driver {

/**
 * @brief Common interface to apply IIR filters right after decoder execution (decoding and
 * resampling)
 */
class Equalizer {
 public:
  /**
   * @brief Construct a new Equalizer object
   */
  Equalizer() = default;

  /**
   * @brief Destroy the Equalizer object
   */
  virtual ~Equalizer() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Set volume on playback stream
   *
   * @param value Desired volume (in a range between 0.f and 1.f)
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code SetVolume(model::Volume value) = 0;

  /**
   * @brief Get volume from playback stream
   * @return model::Volume Volume percentage (in a range between 0.f and 1.f)
   */
  virtual model::Volume GetVolume() const = 0;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_BASE_EQUALIZER_H_
