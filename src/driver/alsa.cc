#include "driver/alsa.h"

namespace driver {

AlsaSound::AlsaSound() : pcm_handle_(nullptr) { CreatePlaybackStream(); }

/* ********************************************************************************************** */

void AlsaSound::CreatePlaybackStream() {
  snd_pcm_t* handle;
  int result = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

  pcm_handle_.reset(std::move(handle));
}

}  // namespace driver