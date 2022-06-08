/**
 * \file
 * \brief  Class for ALSA driver support
 */

#ifndef INCLUDE_DRIVER_ALSA_H_
#define INCLUDE_DRIVER_ALSA_H_

#include <alsa/asoundlib.h>

#include <memory>

#include "model/application_error.h"
#include "model/song.h"

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
 public:
  // TODO: document
  error::Code CreatePlaybackStream();

  error::Code ConfigureParameters(int& period_size);

  error::Code AudioCallback(void* buffer, int buffer_size, int out_samples);

  /* ******************************************************************************************** */
  //! Default Constants
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

  PcmPlayback playback_handle_;
  snd_pcm_uframes_t period_size_;
};

}  // namespace driver
#endif  // INCLUDE_DRIVER_ALSA_H_