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
class Alsa final : public audio::Playback {
 public:
  /**
   * @brief Construct a new Alsa object
   */
  Alsa() = default;

  /**
   * @brief Destroy the Alsa object
   */
  ~Alsa() override = default;

  /* ******************************************************************************************** */
  //! Public API
  /**
   * @brief Create a Playback Stream using ALSA API (current one, if any, is released even when it
   * fails)
   * @param device Name of output device (when empty, the most suitable one is chosen)
   * @return error::Code Playback error converted to application error code
   */
  error::Code CreatePlaybackStream(const std::string& device) override;

  /**
   * @brief List output devices from ALSA API that are meant to be chosen by user
   * @return model::AudioDevices Output devices
   */
  model::AudioDevices ListDevices() const override;

  /**
   * @brief Get name of output device used by playback stream
   * @return std::string Name of output device
   */
  std::string GetDevice() const override { return device_in_use_; }

  /**
   * @brief Configure Playback Stream parameters (sample format, etc...) using ALSA API. Sample rate
   * is not converted by ALSA, so the closest one supported by output device is used
   * @param desired Format of audio samples that would be sent to playback stream, if supported
   * @return error::Code Playback error converted to application error code
   */
  error::Code ConfigureParameters(const model::AudioFormat& desired) override;

  /**
   * @brief Get format of audio samples expected by playback stream
   * @return model::AudioFormat Format of audio samples
   */
  model::AudioFormat GetFormat() const override { return format_; }

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

  /**
   * @brief Set hardware and software parameters on playback stream, using the closest format to the
   * desired one that is supported by output device
   * @param desired Format of audio samples that would be sent to playback stream, if supported
   * @return error::Code Playback error converted to application error code
   */
  error::Code SetParameters(const model::AudioFormat& desired);

  /* ******************************************************************************************** */
  //! Default Constants for Audio Parameters
  static constexpr const char kSelemName[] = "Master";
  static constexpr unsigned int kLatency = 92900;       //!< Overall latency (in microseconds)
  static constexpr unsigned int kPeriodsPerBuffer = 4;  //!< Buffer is split into these periods

  /* ******************************************************************************************** */
  //! Custom declarations with deleters
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

  PcmPlayback playback_handle_;  //! Playback stream handled by ALSA API
  MixerControl mixer_;           //! High level control interface from ALSA API (to manage volume)
  snd_pcm_uframes_t period_size_ = 0;  //! Period size (necessary in order to discover buffer size)
  bool stream_ready_ = false;          //! Current playback stream is ready to play

  std::string device_;         //! Name of output device asked to create playback stream
  std::string device_in_use_;  //! Name of output device used by playback stream
  model::AudioFormat format_;  //! Format of audio samples expected by playback stream
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DRIVER_ALSA_H_
