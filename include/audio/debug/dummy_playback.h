/**
 * \file
 * \brief Interface class for playback support
 */

#ifndef INCLUDE_AUDIO_DEBUG_DUMMY_PLAYBACK_H_
#define INCLUDE_AUDIO_DEBUG_DUMMY_PLAYBACK_H_

#include "audio/base/playback.h"
#include "model/application_error.h"
#include "model/volume.h"

namespace driver {

/**
 * @brief Dummy
 */
class DummyPlayback : public Playback {
 public:
  /**
   * @brief Construct a new Playback object
   */
  DummyPlayback() = default;

  /**
   * @brief Destroy the Playback object
   */
  ~DummyPlayback() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Create a Playback Stream
   * @return error::Code Playback error converted to application error code
   */
  error::Code CreatePlaybackStream() override { return error::kSuccess; }

  /**
   * @brief Configure Playback Stream parameters (sample format, etc...)
   * @return error::Code Playback error converted to application error code
   */
  error::Code ConfigureParameters() override { return error::kSuccess; }

  /**
   * @brief Make playback stream ready to play
   * @return error::Code Playback error converted to application error code
   */
  error::Code Prepare() override { return error::kSuccess; }

  /**
   * @brief Pause current song on playback stream
   * @return error::Code Playback error converted to application error code
   */
  error::Code Pause() override { return error::kSuccess; }

  /**
   * @brief Stop playing song on playback stream
   * @return error::Code Playback error converted to application error code
   */
  error::Code Stop() override { return error::kSuccess; }

  /**
   * @brief Directly write audio buffer to playback stream (this should be called by decoder)
   *
   * @param buffer Audio data buffer
   * @param size Buffer size
   * @return error::Code Playback error converted to application error code
   */
  error::Code AudioCallback(void* buffer, int size) override { return error::kSuccess; }

  /**
   * @brief Set volume on playback stream
   *
   * @param value Desired volume (in a range between 0.f and 1.f)
   * @return error::Code Playback error converted to application error code
   */
  error::Code SetVolume(model::Volume value) override { return error::kSuccess; }

  /**
   * @brief Get volume from playback stream
   * @return model::Volume Volume percentage (in a range between 0.f and 1.f)
   */
  model::Volume GetVolume() override { return model::Volume(); }

  /**
   * @brief Get period size
   * @return uint32_t Period size
   */
  uint32_t GetPeriodSize() const override { return kPeriodSize; }

  /* ******************************************************************************************** */
  //! Constants
 private:
  static constexpr uint32_t kPeriodSize = 1024;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DEBUG_DUMMY_PLAYBACK_H_
