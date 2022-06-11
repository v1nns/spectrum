/**
 * \file
 * \brief  Class to support using ALSA driver
 */

#ifndef INCLUDE_DRIVER_ALSA_H_
#define INCLUDE_DRIVER_ALSA_H_

#include <alsa/asoundlib.h>

#include <memory>

#include "model/application_error.h"

namespace driver {

/**
 * @brief Provides an interface to use ALSA library for handling audio with hardware
 */
class Alsa {
 public:
  /**
   * @brief Construct a new Alsa object
   */
  Alsa();

  /**
   * @brief Destroy the Alsa object
   */
  virtual ~Alsa() = default;

  /* ******************************************************************************************** */
  //! Public API
 public:
  /**
   * @brief Create a Playback Stream using ALSA API
   * @return error::Code Playback error converted to application error code
   */
  error::Code CreatePlaybackStream();

  /**
   * @brief Configure Playback Stream parameters (sample format, etc...) using ALSA API
   * @return error::Code Playback error converted to application error code
   */
  error::Code ConfigureParameters();

  /**
   * @brief Ask ALSA API to make playback stream ready to play
   * @return error::Code Playback error converted to application error code
   */
  error::Code Prepare();

  /**
   * @brief Pause current song on playback stream
   * @return error::Code Playback error converted to application error code
   */
  error::Code Pause();

  /**
   * @brief Stop playing song on playback stream
   * @return error::Code Playback error converted to application error code
   */
  error::Code Stop();

  /**
   * @brief Directly write audio buffer to playback stream (this should be called by decoder)
   *
   * @param buffer Audio data buffer
   * @param buffer_size Maximum size for buffer
   * @param out_samples Actual size for buffer
   * @return error::Code Playback error converted to application error code
   */
  error::Code AudioCallback(void* buffer, int buffer_size, int out_samples);

  /**
   * @brief Get period size (previously filled by ALSA API)
   * @return uint32_t Period size
   */
  uint32_t GetPeriodSize() const { return period_size_; }

  /* ******************************************************************************************** */
  //! Default Constants for Audio Parameters
 private:
  static constexpr const char kDevice[] = "default";
  static constexpr int kChannels = 2;
  static constexpr int kSampleRate = 44100;
  static constexpr snd_pcm_format_t kSampleFormat = SND_PCM_FORMAT_S16_LE;

  /* ******************************************************************************************** */
  //! Custom declarations with deleters
 private:
  struct PcmDeleter {
    void operator()(snd_pcm_t* p) const {
      snd_pcm_drain(p);
      snd_pcm_close(p);
    }
  };

  using PcmPlayback = std::unique_ptr<snd_pcm_t, PcmDeleter>;

  /* ******************************************************************************************** */
  //! Variables

  PcmPlayback playback_handle_;    //! Playback stream handled by ALSA API
  snd_pcm_uframes_t period_size_;  //! Period size (necessary in order to discover buffer size)
};

}  // namespace driver
#endif  // INCLUDE_DRIVER_ALSA_H_