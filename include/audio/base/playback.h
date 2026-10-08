/**
 * \file
 * \brief Interface class for playback support
 */

#ifndef INCLUDE_AUDIO_BASE_PLAYBACK_H_
#define INCLUDE_AUDIO_BASE_PLAYBACK_H_

#include <cstdint>
#include <string>

#include "model/application_error.h"
#include "model/audio_device.h"
#include "model/audio_format.h"
#include "model/volume.h"

namespace audio {

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
   * @brief Create a Playback Stream (current one, if any, is released even when it fails)
   * @param device Name of output device (when empty, the most suitable one is chosen)
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code CreatePlaybackStream(const std::string& device) = 0;

  /**
   * @brief List output devices available to create a Playback Stream
   * @return model::AudioDevices Output devices
   */
  virtual model::AudioDevices ListDevices() const = 0;

  /**
   * @brief Configure Playback Stream parameters (sample format, etc...). As output device may not
   * support the desired format, use GetFormat() to know which one must be sent to it
   * @param desired Format of audio samples that would be sent to playback stream, if supported
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code ConfigureParameters(const model::AudioFormat& desired) = 0;

  /**
   * @brief Get format of audio samples expected by playback stream (the closest one to the desired
   * format that is supported by output device)
   * @return model::AudioFormat Format of audio samples
   */
  virtual model::AudioFormat GetFormat() const = 0;

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
   * @param size Buffer size
   * @return error::Code Playback error converted to application error code
   */
  virtual error::Code AudioCallback(void* buffer, int size) = 0;

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

}  // namespace audio
#endif  // INCLUDE_AUDIO_BASE_PLAYBACK_H_
