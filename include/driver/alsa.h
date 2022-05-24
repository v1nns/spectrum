/**
 * \file
 * \brief  Class for ALSA driver support
 */

#ifndef INCLUDE_DRIVER_ALSA_H_
#define INCLUDE_DRIVER_ALSA_H_

#include <alsa/asoundlib.h>

#include <memory>

namespace driver {

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

  void CreatePlaybackStream();

  /* ******************************************************************************************** */
 private:
  struct Closer {
    void operator()(snd_pcm_t* p) const { snd_pcm_close(p); }
  };

  std::unique_ptr<snd_pcm_t, Closer> pcm_handle_;
};

}  // namespace driver
#endif  // INCLUDE_DRIVER_ALSA_H_