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

//! Callback to provide data for ALSA to playback
using PlaybackDataCallback = std::function<int(long)>;

/**
 * @brief TODO:...
 */
class AlsaSound {
 public:
  /**
   * @brief Construct a new AlsaSound object
   */
  AlsaSound();

  /**
   * @brief Destroy the AlsaSound object
   */
  virtual ~AlsaSound() = default;

  // TODO: document
  error::Code Initialize();
  error::Code SetupAudioParameters(const model::AudioData& audio_info);

  void RegisterDataCallback(PlaybackDataCallback cb);

  /* ******************************************************************************************** */
 private:
  static constexpr uint16_t kBufferSize = 4096;

  // TODO: document
  void CreatePlaybackStream();

  void ConfigureHardwareParams(const model::AudioData& audio_info);
  void ConfigureSoftwareParams();

  /* ******************************************************************************************** */
 private:
  struct Closer {
    void operator()(snd_pcm_t* p) const { snd_pcm_close(p); }
  };

  std::unique_ptr<snd_pcm_t, Closer> playback_handle_;
  PlaybackDataCallback cb_data_;
};

}  // namespace driver
#endif  // INCLUDE_DRIVER_ALSA_H_