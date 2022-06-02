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

  // TODO: document
  error::Code Initialize();
  error::Code SetupAudioParameters(const model::AudioData& audio_info);

  // TODO: media control
  error::Code Prepare();
  error::Code Play(const std::vector<double>& data);
  error::Code Stop();

  /* ******************************************************************************************** */
 private:
  static constexpr const char kDevice[] = "default";
  static constexpr uint16_t kBufferSize = 4096;

  // TODO: document
  void CreatePlaybackStream();

  error::Code ConfigureHardwareParams(const model::AudioData& audio_info);
  error::Code ConfigureSoftwareParams();

  // TODO: util (and this is considering a WAV file, where it is always in little-endian)
  snd_pcm_format_t GetPcmFormat(uint32_t bit_depth);

  /* ******************************************************************************************** */
 private:
  struct Deleter {
    void operator()(snd_pcm_t* p) const { snd_pcm_close(p); }
  };

  std::unique_ptr<snd_pcm_t, Deleter> playback_handle_;
  long buffer_index_;
};

}  // namespace driver
#endif  // INCLUDE_DRIVER_ALSA_H_