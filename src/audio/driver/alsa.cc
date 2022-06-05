#include "audio/driver/alsa.h"

#include <algorithm>
#include <iostream>

namespace driver {

Alsa::Alsa() : playback_handle_{}, buffer_index_{} {}

/* ********************************************************************************************** */

error::Code Alsa::Initialize() {
  CreatePlaybackStream();

  return error::kSuccess;
}
/* ********************************************************************************************** */

error::Code Alsa::SetupAudioParameters(const model::AudioData& audio_info) {
  error::Code result = error::kSetupAudioParamsFailed;

  result = ConfigureHardwareParams(audio_info);
  if (result != error::kSuccess) return result;

  result = ConfigureSoftwareParams();
  return result;
}

/* ********************************************************************************************** */

void Alsa::CreatePlaybackStream() {
  snd_pcm_t* handle;
  int result = snd_pcm_open(&handle, kDevice, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);

  if (result < 0) {
    fprintf(stderr, "cannot open audio device \"default\" (%s)\n", snd_strerror(result));
    exit(1);
  }

  playback_handle_.reset(std::move(handle));
}

/* ********************************************************************************************** */

error::Code Alsa::ConfigureHardwareParams(const model::AudioData& audio_info) {
  error::Code result = error::kSuccess;
  int err = 0;
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

  // SND_PCM_FORMAT_S16_LE
  const auto pcm_format = GetPcmFormat(audio_info.bit_depth);
  if ((err = snd_pcm_hw_params_set_format(playback_handle_.get(), hw_params, pcm_format)) < 0) {
    fprintf(stderr, "cannot set sample format (%s)\n", snd_strerror(err));
    exit(1);
  }

  unsigned int rate = audio_info.sample_rate;
  if ((err = snd_pcm_hw_params_set_rate_near(playback_handle_.get(), hw_params, &rate, 0)) < 0) {
    fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_channels(playback_handle_.get(), hw_params,
                                            audio_info.num_channels)) < 0) {
    fprintf(stderr, "cannot set channel count (%s)\n", snd_strerror(err));
    exit(1);
  }

  //   if ((err = snd_pcm_hw_set_periods(playback_handle_.get(), hw_params, periods, 0)) < 0) {
  //     fprintf(stderr, "cannot set periods\n", snd_strerror(err));
  //     exit(1);
  //   }

  if ((err = snd_pcm_hw_params(playback_handle_.get(), hw_params)) < 0) {
    fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
    exit(1);
  }

  snd_pcm_hw_params_free(hw_params);
  return result;
}

/* ********************************************************************************************** */

error::Code Alsa::ConfigureSoftwareParams() {
  error::Code result = error::kSuccess;
  snd_pcm_sw_params_t* sw_params;
  int err = 0;

  // Notify ALSA to wake us up whenever 4096 or more frames of playback data can be delivered. Also,
  // tell ALSA that we'll start the device ourselves.
  if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
    fprintf(stderr, "cannot allocate software parameters structure (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_sw_params_current(playback_handle_.get(), sw_params)) < 0) {
    fprintf(stderr, "cannot initialize software parameters structure (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_sw_params_set_avail_min(playback_handle_.get(), sw_params, kBufferSize)) < 0) {
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

  return result;
}

/* ********************************************************************************************** */

error::Code Alsa::Prepare() {
  int err = 0;

  if ((err = snd_pcm_prepare(playback_handle_.get())) < 0) {
    fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
    exit(1);
  }
  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Play(const std::vector<double>& data) {
  int err = 0;
  snd_pcm_sframes_t frame_size;

  std::cout << "vou comecar a tocar, samples: " << data.size() << std::endl;

  buffer_index_ = 0;
  while (buffer_index_ <= data.size()) {
    // wait till the interface is ready for data, or 1 second has elapsed.
    if ((err = snd_pcm_wait(playback_handle_.get(), 1000)) < 0) {
      std::cout << "poll failed " << strerror(errno) << std::endl;
      return error::kUnknownError;
    }

    // find out how much space is available for playback data
    if ((frame_size = snd_pcm_avail_update(playback_handle_.get())) < 0) {
      if (frame_size == -EPIPE) {
        std::cout << "an xrun occured" << std::endl;
        return error::kUnknownError;
      } else {
        std::cout << "unknown ALSA avail update return value: " << frame_size << std::endl;
        return error::kUnknownError;
      }
    }

    frame_size = frame_size > kBufferSize ? kBufferSize : frame_size;

    // deliver the data
    auto first = data.begin() + buffer_index_;
    auto last = first + frame_size;

    std::vector<double> buf(first, last);
    buffer_index_ += frame_size;

    //   for (size_t i = buffer_index_; i < (buffer_index_ + frame_size); i += 2) {}

    if ((err = snd_pcm_writei(playback_handle_.get(), &buf[0], frame_size)) < 0) {
      std::cout << "write failed: " << snd_strerror(err) << std::endl;
      return error::kUnknownError;
    }
  }

  std::cout << "ue, cheguei ao final" << std::endl;
  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Stop() {
  snd_pcm_drop(playback_handle_.get());
  buffer_index_ = 0;
  return error::kSuccess;
}

/* ********************************************************************************************** */

snd_pcm_format_t Alsa::GetPcmFormat(uint32_t bit_depth) {
  switch (bit_depth) {
    case 8:
      return SND_PCM_FORMAT_U8;

    case 16:
      return SND_PCM_FORMAT_S16_LE;

    case 24:
      return SND_PCM_FORMAT_S24_LE;

    case 32:
      return SND_PCM_FORMAT_S32_LE;

    default:
      return SND_PCM_FORMAT_UNKNOWN;
  }
}

}  // namespace driver