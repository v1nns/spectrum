/**
 * \file
 * \brief  Class to support using ALSA driver
 */

#ifndef INCLUDE_AUDIO_DRIVER_ALSA_H_
#define INCLUDE_AUDIO_DRIVER_ALSA_H_

#include <alsa/asoundlib.h>

#include <memory>

#include "audio/base/playback.h"
#include "model/application_error.h"

namespace driver {

/**
 * @brief Provides an interface to use ALSA library for handling audio with hardware
 */
class Alsa : public Playback {
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
  error::Code CreatePlaybackStream() override;

  /**
   * @brief Configure Playback Stream parameters (sample format, etc...) using ALSA API
   * @return error::Code Playback error converted to application error code
   */
  error::Code ConfigureParameters() override;

  /**
   * @brief Ask ALSA API to make playback stream ready to play
   * @return error::Code Playback error converted to application error code
   */
  error::Code Prepare() override;

  /**
   * @brief Pause current song on playback stream
   * @return error::Code Playback error converted to application error code
   */
  error::Code Pause() override;

  /**
   * @brief Stop playing song on playback stream
   * @return error::Code Playback error converted to application error code
   */
  error::Code Stop() override;

  /**
   * @brief Directly write audio buffer to playback stream (this should be called by decoder)
   *
   * @param buffer Audio data buffer
   * @param size Buffer size
   * @return error::Code Playback error converted to application error code
   */
  error::Code AudioCallback(void* buffer, int size) override;

  /**
   * @brief Set volume on playback stream
   *
   * @param value Desired volume (in a range between 0.f and 1.f)
   * @return error::Code Playback error converted to application error code
   */
  error::Code SetVolume(model::Volume value) override;

  /**
   * @brief Get volume from playback stream
   * @return model::Volume Volume percentage (in a range between 0.f and 1.f)
   */
  model::Volume GetVolume() override;

  /**
   * @brief Get period size (previously filled by ALSA API)
   * @return uint32_t Period size
   */
  uint32_t GetPeriodSize() const override { return (uint32_t)period_size_; }

  /* ******************************************************************************************** */
  //! Utility
 private:
  /**
   * @brief Find and return master playback from High level control interface from ALSA (p.s.: not
   * necessary the use of smart pointers here because this resource is managed by ALSA)
   */
  snd_mixer_elem_t* GetMasterPlayback();

  /* ******************************************************************************************** */
  //! Default Constants for Audio Parameters
 private:
  static constexpr const char kDevice[] = "default";
  static constexpr const char kSelemName[] = "Master";
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

  struct MixerDeleter {
    void operator()(snd_mixer_t* p) const { snd_mixer_close(p); }
  };

  using PcmPlayback = std::unique_ptr<snd_pcm_t, PcmDeleter>;

  using MixerControl = std::unique_ptr<snd_mixer_t, MixerDeleter>;

  /* ******************************************************************************************** */
  //! Variables

  PcmPlayback playback_handle_;    //! Playback stream handled by ALSA API
  MixerControl mixer_;             //! High level control interface from ALSA API (to manage volume)
  snd_pcm_uframes_t period_size_;  //! Period size (necessary in order to discover buffer size)
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DRIVER_ALSA_H_
