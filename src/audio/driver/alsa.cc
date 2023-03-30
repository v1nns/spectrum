#include "audio/driver/alsa.h"

#include <alsa/mixer.h>
#include <math.h>

#include "model/application_error.h"
#include "util/logger.h"

namespace driver {

Alsa::Alsa() : playback_handle_{}, mixer_{}, period_size_{} {}

/* ********************************************************************************************** */

error::Code Alsa::CreatePlaybackStream() {
  LOG("Create new playback stream");

  // Create playback stream on ALSA
  snd_pcm_t *pcm_handle = nullptr;
  if (snd_pcm_open(&pcm_handle, kDevice, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    ERROR("Cannot open playback stream");
    return error::kUnknownError;
  }

  playback_handle_.reset(std::move(pcm_handle));

  // Create mixer to control volume on ALSA
  snd_mixer_t *mixer_handle = nullptr;

  snd_mixer_open(&mixer_handle, 0);
  snd_mixer_attach(mixer_handle, kDevice);
  snd_mixer_selem_register(mixer_handle, nullptr, nullptr);

  if (snd_mixer_load(mixer_handle) < 0) {
    ERROR("Cannot create mixer for control");
    return error::kUnknownError;
  }

  mixer_.reset(std::move(mixer_handle));

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::ConfigureParameters() {
  LOG("Configure parameters on playback stream");

  // with latency as 92900us, we get a period size equal to 1024
  if (snd_pcm_set_params(playback_handle_.get(), kSampleFormat, SND_PCM_ACCESS_RW_INTERLEAVED,
                         kChannels, kSampleRate, 0, 92900) < 0) {
    ERROR("Cannot set parameters on playback stream");
    return error::kUnknownError;
  }

  snd_pcm_uframes_t buffer_size = 0;
  if (snd_pcm_get_params(playback_handle_.get(), &buffer_size, &period_size_) < 0) {
    ERROR("Cannot get parameters from playback stream");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Prepare() {
  LOG("Prepare playback stream to play audio");

  if (snd_pcm_prepare(playback_handle_.get()) < 0) {
    ERROR("Cannot prepare playback stream");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Pause() {
  LOG("Pause playback stream");

  if (snd_pcm_drop(playback_handle_.get()) < 0) {
    ERROR("Cannot pause playback stream and clear remaining frames on buffer");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Stop() {
  LOG("Stop playback stream");

  if (snd_pcm_drain(playback_handle_.get()) < 0) {
    ERROR("Cannot stop playback stream and preserve remaining frames on buffer");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::AudioCallback(void *buffer, int size) {
  // As this is called multiple times, LOG will not be called here in the beginning
  if (auto result = snd_pcm_writei(playback_handle_.get(), buffer, size); result < 0) {
    ERROR("Cannot write buffer to playback stream, error=", result);
    if ((result = snd_pcm_recover(playback_handle_.get(), result, 1)) == 0) {
      // TODO: do something?
      LOG("Recovered playback stream from error (overrun/underrun), error=", result);
    }
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

snd_mixer_elem_t *Alsa::GetMasterPlayback() {
  LOG("Use mixer to get master playback");

  // Select master playback
  snd_mixer_selem_id_t *sid = nullptr;

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_name(sid, kSelemName);
  // snd_mixer_selem_id_set_index(sid, 0);

  snd_mixer_elem_t *elem = snd_mixer_find_selem(mixer_.get(), sid);
  return elem;
}

/* ********************************************************************************************** */

error::Code Alsa::SetVolume(model::Volume value) {
  LOG("Set volume on master playback with value=", value);

  auto master = GetMasterPlayback();
  if (master == nullptr) {
    ERROR("Cannot get master playback");
    return error::kUnknownError;
  }

  // Get volume range
  long min, max;
  snd_mixer_selem_get_playback_volume_range(master, &min, &max);

  // Calculate new volume based on values read
  long new_value = (max * (float)value) - min;

  // Set new value
  snd_mixer_selem_set_playback_volume_all(master, new_value);
  return error::kSuccess;
}

/* ********************************************************************************************** */

model::Volume Alsa::GetVolume() {
  LOG("Get volume from master playback");

  auto master = GetMasterPlayback();
  if (master == nullptr) {
    ERROR("Cannot get master playback");
    return model::Volume();
  }

  // Get value range for volume
  long min, max;
  snd_mixer_selem_get_playback_volume_range(master, &min, &max);

  // Get current value from master's channel
  long current;
  snd_mixer_selem_get_playback_volume(master, SND_MIXER_SCHN_MONO, &current);

  // Convert it to percentage and round the resulted value
  float percent = 100.0f * (((float)current - min) / (max - min));
  float rounded = roundf(percent) / 100;

  return model::Volume(rounded);
}

}  // namespace driver
