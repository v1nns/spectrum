#include "audio/driver/alsa.h"

namespace driver {

Alsa::Alsa() : playback_handle_{}, period_size_{} {}

/* ********************************************************************************************** */

error::Code Alsa::CreatePlaybackStream() {
  snd_pcm_t* handle = nullptr;
  if (snd_pcm_open(&handle, kDevice, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    return error::kUnknownError;
  }

  playback_handle_.reset(std::move(handle));

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::ConfigureParameters() {
  if (snd_pcm_set_params(playback_handle_.get(), kSampleFormat, SND_PCM_ACCESS_RW_INTERLEAVED,
                         kChannels, kSampleRate, 0, kSampleRate / 4) < 0) {
    return error::kUnknownError;
  }

  snd_pcm_uframes_t buffer_size = 0;
  if (snd_pcm_get_params(playback_handle_.get(), &buffer_size, &period_size_) < 0) {
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Prepare() {
  if (snd_pcm_prepare(playback_handle_.get()) < 0) {
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Stop() {
  if (snd_pcm_drain(playback_handle_.get()) < 0) {
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::AudioCallback(void* buffer, int buffer_size, int out_samples) {
  int ret = snd_pcm_writei(playback_handle_.get(), buffer, out_samples);

  if (ret < 0) {
    if ((ret = snd_pcm_recover(playback_handle_.get(), ret, 1)) == 0) {
      // TODO: do something?
      // std::cout << "AlsaPlayer: recovered after xrun (overrun/underrun)" << std::endl;
    }
  }

  return error::kSuccess;
}

}  // namespace driver