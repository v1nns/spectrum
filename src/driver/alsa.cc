#include "driver/alsa.h"

namespace driver {

AlsaSound::AlsaSound() : playback_handle_(nullptr), cb_data_(nullptr) {}

/* ********************************************************************************************** */

error::Code AlsaSound::Initialize() {
  CreatePlaybackStream();

  ConfigureHardwareParams();

  ConfigureSoftwareParams();

  // Start and Stop
  //   snd_pcm_drop();
  //   snd_pcm_prepare();

  return error::kSuccess;
}

/* ********************************************************************************************** */

void AlsaSound::RegisterDataCallback(PlaybackDataCallback cb) { cb_data_ = cb; }

/* ********************************************************************************************** */

void AlsaSound::CreatePlaybackStream() {
  snd_pcm_t* handle;
  int result = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

  if (result < 0) {
    fprintf(stderr, "cannot open audio device \"default\" (%s)\n", snd_strerror(result));
    exit(1);
  }

  playback_handle_.reset(std::move(handle));
}

/* ********************************************************************************************** */

void AlsaSound::ConfigureHardwareParams() {
  int err;
  snd_pcm_hw_params_t* hw_params;
  if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
    fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_any(playback_handle_.get(), hw_params)) < 0) {
    fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_access(playback_handle_.get(), hw_params,
                                          SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf(stderr, "cannot set access type (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_format(playback_handle_.get(), hw_params,
                                          SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf(stderr, "cannot set sample format (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_rate(playback_handle_.get(), hw_params, 44100, 0)) < 0) {
    fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_channels(playback_handle_.get(), hw_params, 2)) < 0) {
    fprintf(stderr, "cannot set channel count (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params(playback_handle_.get(), hw_params)) < 0) {
    fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
    exit(1);
  }

  snd_pcm_hw_params_free(hw_params);
}

/* ********************************************************************************************** */

void AlsaSound::ConfigureSoftwareParams() {
  int err;
  snd_pcm_sw_params_t* sw_params;
  /* tell ALSA to wake us up whenever 4096 or more frames of playback data can be delivered. Also,
   * tell ALSA that we'll start the device ourselves.*/

  if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
    fprintf(stderr, "cannot allocate software parameters structure (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_sw_params_current(playback_handle_.get(), sw_params)) < 0) {
    fprintf(stderr, "cannot initialize software parameters structure (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_sw_params_set_avail_min(playback_handle_.get(), sw_params, 4096)) < 0) {
    fprintf(stderr, "cannot set minimum available count (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_sw_params_set_start_threshold(playback_handle_.get(), sw_params, 0U)) < 0) {
    fprintf(stderr, "cannot set start mode (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_sw_params(playback_handle_.get(), sw_params)) < 0) {
    fprintf(stderr, "cannot set software parameters (%s)\n", snd_strerror(err));
    exit(1);
  }

  snd_pcm_sw_params_free(sw_params);

  /* the interface will interrupt the kernel every 4096 frames, and ALSA
     will wake up this program very soon after that.
  */
  if ((err = snd_pcm_prepare(playback_handle_.get())) < 0) {
    fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
    exit(1);
  }
}

}  // namespace driver