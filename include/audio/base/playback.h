/**
 * \file
 * \brief Interface class for playback support
 */

#ifndef INCLUDE_AUDIO_BASE_PLAYBACK_H_
#define INCLUDE_AUDIO_BASE_PLAYBACK_H_

#include "model/application_error.h"
#include "model/volume.h"

namespace driver {

/**
 * @brief Common interface to create and handle playback audio stream
 */
class Playback {
 public:
  /**
   * @brief Construct a new Playback object
   */
  Playback() = default;

  /**
   * @brief Destroy the Playback object
   */
  virtual ~Playback() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Create a Playback Stream
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code CreatePlaybackStream() = 0;

  /**
   * @brief Configure Playback Stream parameters (sample format, etc...)
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code ConfigureParameters() = 0;

  /**
   * @brief Make playback stream ready to play
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code Prepare() = 0;

  /**
   * @brief Pause current song on playback stream
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code Pause() = 0;

  /**
   * @brief Stop playing song on playback stream
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code Stop() = 0;

  /**
   * @brief Directly write audio buffer to playback stream (this should be called by decoder)
   *
   * @param buffer Audio data buffer
   * @param max_size Maximum size for buffer
   * @param actual_size Actual size for buffer
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code AudioCallback(void* buffer, int max_size, int actual_size) = 0;

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
  virtual model::Volume GetVolume() = 0;

  /**
   * @brief Get period size
   * @return uint32_t Period size
   */
  virtual uint32_t GetPeriodSize() const = 0;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_BASE_PLAYBACK_H_
