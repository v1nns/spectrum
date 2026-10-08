#include "audio/driver/alsa.h"

#include <alsa/mixer.h>
#include <math.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <iomanip>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "model/application_error.h"
#include "util/logger.h"

namespace driver {

namespace {

//! Devices listed by ALSA with these names (or starting with them, when followed by its card) are
//! the ones meant to be chosen by user, the remaining are plugins or variations of them
constexpr std::array<std::string_view, 9> kDeviceNames = {
    "default", "pipewire", "pulse", "sysdefault", "front", "hw", "plughw", "iec958", "hdmi",
};

//! After playing through a sound server, hardware is still held by it for a few seconds (until it
//! is considered idle), so opening the same hardware directly must wait for it to be released
constexpr std::chrono::seconds kReleaseTimeout{6};
constexpr std::chrono::milliseconds kReleaseInterval{200};

//! Devices to use when none is chosen by user, sorted by priority
constexpr std::array<std::string_view, 2> kPreferedDeviceNames = {"default", "pulse"};

//! Write errors from ALSA library to log (otherwise, they are printed on terminal over the UI)
void LogLibraryError(const char* /*file*/, int /*line*/, const char* function, int /*errcode*/,
                     const char* format, ...) {
  std::array<char, 256> message{};

  va_list args;
  va_start(args, format);
  vsnprintf(message.data(), message.size(), format, args);
  va_end(args);

  WARN("ALSA library: ", message.data(), " (", function, ")");
}

//! Get sample format from ALSA that is equivalent to the given one (using native endianness, as
//! samples are created by decoder like that)
snd_pcm_format_t ToPcmFormat(model::SampleFormat format) {
  return format == model::SampleFormat::S16 ? SND_PCM_FORMAT_S16 : SND_PCM_FORMAT_S32;
}

//! Check if device is one of those meant to be chosen by user (instead of a plugin, like the ones
//! to convert sample rate, or a variation of another device)
bool IsDeviceForUser(std::string_view name) {
  // Name may be followed by card and device (e.g. "front:CARD=PCH,DEV=0")
  name = name.substr(0, name.find(':'));

  return std::find(kDeviceNames.begin(), kDeviceNames.end(), name) != kDeviceNames.end();
}

//! Get text from hint and release it
std::string GetHint(const void* hint, const char* id) {
  char* value = snd_device_name_get_hint(hint, id);
  if (!value) return "";

  std::string text{value};
  free(value);

  return text;
}

//! List audio devices to play songs (including the ones not meant to be chosen by user)
model::AudioDevices ListOutputDevices() {
  model::AudioDevices devices;

  void** hints = nullptr;
  if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
    ERROR("Cannot get device name hints");
    return devices;
  }

  for (void** hint = hints; *hint; hint++) {
    std::string name = GetHint(*hint, "NAME");

    // Direction is empty when device is used for both of them
    if (std::string direction = GetHint(*hint, "IOID");
        name.empty() || (!direction.empty() && direction != "Output")) {
      continue;
    }

    // Description may contain more than one line
    std::string description = GetHint(*hint, "DESC");
    std::replace(description.begin(), description.end(), '\n', ' ');

    devices.push_back(model::AudioDevice{.name = name, .description = description});
  }

  snd_device_name_free_hint(hints);
  return devices;
}

//! Get a list of prefered devices to use, sorted by priority
std::vector<std::string> GetPreferedDevicesName() {
  std::vector<std::string> devices_names;

  // A plugin may be opened like any device, but it is not able to play audio by itself (or not in
  // any format), so it must not be used only because the prefered devices are not available
  for (const auto& device : ListOutputDevices()) {
    if (IsDeviceForUser(device.name)) devices_names.push_back(device.name);
  }

  if (devices_names.empty()) {
    ERROR("No audio device found");
    return devices_names;
  }

  auto iterator_begin = devices_names.begin();
  for (auto& prefered_device : kPreferedDeviceNames) {
    auto it = std::find(devices_names.begin(), devices_names.end(), prefered_device);
    if (it == devices_names.end()) {
      continue;
    }

    std::iter_swap(iterator_begin, it);
    iterator_begin++;
  }

  return devices_names;
}

}  // namespace

error::Code Alsa::CreatePlaybackStream(const std::string& device) {
  LOG("Create new playback stream on device=", std::quoted(device));
  device_ = device;
  snd_lib_error_set_handler(&LogLibraryError);

  // Current playback stream must be released first, as it may be using the same hardware (which
  // cannot be opened again, when it is used directly)
  mixer_.reset();
  playback_handle_.reset();
  device_in_use_.clear();

  // Hardware may be busy for a while only when there was a playback stream ready to play (which
  // may be linked to it by a sound server) and a device was chosen
  const bool wait_release = std::exchange(stream_ready_, false) && !device.empty();
  const auto deadline = std::chrono::steady_clock::now() + kReleaseTimeout;

  // Use only the given device, unless there is none
  std::vector<std::string> devices_name =
      device.empty() ? GetPreferedDevicesName() : std::vector<std::string>{device};

  // Create playback stream on ALSA
  snd_pcm_t* pcm_handle = nullptr;

  std::string device_name;
  for (auto& name : devices_name) {
    LOG("Creating playback stream on device: ", std::quoted(name));
    int result = snd_pcm_open(&pcm_handle, name.c_str(), SND_PCM_STREAM_PLAYBACK, 0);

    while (result == -EBUSY && wait_release && std::chrono::steady_clock::now() < deadline) {
      LOG("Device is busy, waiting for it to be released: ", std::quoted(name));
      std::this_thread::sleep_for(kReleaseInterval);
      result = snd_pcm_open(&pcm_handle, name.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
    }

    if (result < 0) {
      WARN("Cannot open playback stream on device: ", std::quoted(name),
           ", error=", snd_strerror(result));
      continue;
    }
    INFO("Created playback stream on device: ", std::quoted(name));

    device_name = name;
    break;
  }

  if (device_name.empty()) {
    ERROR("Cannot open playback stream on any device!");
    return error::kOpenDeviceFailed;
  }

  playback_handle_.reset(std::move(pcm_handle));
  device_in_use_ = device_name;

  // Create mixer to control volume on ALSA
  snd_mixer_t* mixer_handle = nullptr;

  if (snd_mixer_open(&mixer_handle, 0) < 0) {
    WARN("Cannot open mixer for control");
    return error::kSuccess;
  }

  mixer_.reset(std::move(mixer_handle));

  // Not every device has a mixer (and it is not necessary to play songs)
  if (snd_mixer_attach(mixer_.get(), device_name.c_str()) < 0 ||
      snd_mixer_selem_register(mixer_.get(), nullptr, nullptr) < 0 ||
      snd_mixer_load(mixer_.get()) < 0) {
    WARN("Cannot create mixer for control on device: ", std::quoted(device_name));
    mixer_.reset();
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

model::AudioDevices Alsa::ListDevices() const {
  model::AudioDevices devices;

  for (auto& device : ListOutputDevices()) {
    if (IsDeviceForUser(device.name)) devices.push_back(std::move(device));
  }

  LOG("Found ", devices.size(), " output devices");
  return devices;
}

/* ********************************************************************************************** */

error::Code Alsa::ConfigureParameters(const model::AudioFormat& desired) {
  LOG("Configure parameters on playback stream, desired format=", desired);
  if (!playback_handle_) return error::kOpenDeviceFailed;

  // Parameters cannot be changed while playback stream has samples to play
  if (stream_ready_) snd_pcm_drop(playback_handle_.get());

  error::Code result = SetParameters(desired);

  // Not every device accepts new parameters after being configured (some of them keep using the
  // previous sample format, even when the desired one is supported), so create its playback stream
  // again
  if (stream_ready_ &&
      (result != error::kSuccess || format_.sample_format != desired.sample_format)) {
    LOG("Create playback stream again to change its parameters");
    const std::string device = device_;

    result = CreatePlaybackStream(device);
    if (result == error::kSuccess) result = SetParameters(desired);
  }

  // Playback stream cannot be used without parameters, so release it (otherwise, writing samples
  // to it is not safe)
  if (result != error::kSuccess) {
    mixer_.reset();
    playback_handle_.reset();
    stream_ready_ = false;
    return result;
  }

  stream_ready_ = true;
  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::SetParameters(const model::AudioFormat& desired) {
  snd_pcm_t* pcm = playback_handle_.get();
  if (!pcm) return error::kOpenDeviceFailed;

  snd_pcm_hw_params_t* hw_params = nullptr;
  snd_pcm_hw_params_alloca(&hw_params);

  if (snd_pcm_hw_params_any(pcm, hw_params) < 0) {
    ERROR("Cannot get parameters supported by playback stream");
    return error::kSetupAudioParamsFailed;
  }

  // Sample rate is converted by decoder (which is better at it), so ALSA must not do it and tell
  // which one is really supported by output device (not every device has this option)
  snd_pcm_hw_params_set_rate_resample(pcm, hw_params, 0);

  if (snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    ERROR("Cannot set access type on playback stream");
    return error::kSetupAudioParamsFailed;
  }

  // Use desired sample format, or any other that is supported by output device
  model::AudioFormat format = desired;
  bool format_supported = false;

  for (auto sample_format :
       {desired.sample_format, model::SampleFormat::S32, model::SampleFormat::S16}) {
    if (snd_pcm_hw_params_set_format(pcm, hw_params, ToPcmFormat(sample_format)) == 0) {
      format.sample_format = sample_format;
      format_supported = true;
      break;
    }
  }

  if (!format_supported) {
    ERROR("Cannot set sample format on playback stream");
    return error::kSetupAudioParamsFailed;
  }

  if (snd_pcm_hw_params_set_channels(pcm, hw_params, desired.channels) < 0) {
    ERROR("Cannot set number of channels on playback stream, channels=", desired.channels);
    return error::kSetupAudioParamsFailed;
  }

  // Use desired sample rate, or the closest one that is supported by output device
  unsigned int sample_rate = desired.sample_rate;
  if (snd_pcm_hw_params_set_rate_near(pcm, hw_params, &sample_rate, nullptr) < 0) {
    ERROR("Cannot set sample rate on playback stream, rate=", desired.sample_rate);
    return error::kSetupAudioParamsFailed;
  }

  format.sample_rate = sample_rate;

  // Period has the same duration for any sample rate (e.g. its size is equal to 1024 for 44.1 kHz),
  // and it is chosen before buffer, as not every device accepts them the other way around
  unsigned int period_time = kLatency / kPeriodsPerBuffer;
  snd_pcm_uframes_t period_size = 0;

  if (snd_pcm_hw_params_set_period_time_near(pcm, hw_params, &period_time, nullptr) < 0 ||
      snd_pcm_hw_params_get_period_size(hw_params, &period_size, nullptr) < 0) {
    ERROR("Cannot set period time on playback stream");
    return error::kSetupAudioParamsFailed;
  }

  snd_pcm_uframes_t buffer_size = period_size * kPeriodsPerBuffer;

  if (snd_pcm_hw_params_set_buffer_size_near(pcm, hw_params, &buffer_size) < 0) {
    ERROR("Cannot set buffer size on playback stream");
    return error::kSetupAudioParamsFailed;
  }

  if (int result = snd_pcm_hw_params(pcm, hw_params); result < 0) {
    ERROR("Cannot set parameters on playback stream, error=", snd_strerror(result));
    return error::kSetupAudioParamsFailed;
  }

  if (snd_pcm_hw_params_get_buffer_size(hw_params, &buffer_size) < 0 ||
      snd_pcm_hw_params_get_period_size(hw_params, &period_size_, nullptr) < 0 ||
      period_size_ == 0) {
    ERROR("Cannot get parameters from playback stream");
    return error::kSetupAudioParamsFailed;
  }

  // Start playing only when buffer is full, and wake up when there is room for a whole period
  snd_pcm_sw_params_t* sw_params = nullptr;
  snd_pcm_sw_params_alloca(&sw_params);

  if (snd_pcm_sw_params_current(pcm, sw_params) < 0 ||
      snd_pcm_sw_params_set_start_threshold(pcm, sw_params,
                                            (buffer_size / period_size_) * period_size_) < 0 ||
      snd_pcm_sw_params_set_avail_min(pcm, sw_params, period_size_) < 0 ||
      snd_pcm_sw_params(pcm, sw_params) < 0) {
    ERROR("Cannot set software parameters on playback stream");
    return error::kSetupAudioParamsFailed;
  }

  format_ = format;
  INFO("Configured playback stream with format=", format_, " period size=", period_size_,
       " buffer size=", buffer_size);

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Prepare() {
  LOG("Prepare playback stream to play audio");
  if (!playback_handle_) return error::kOpenDeviceFailed;

  if (snd_pcm_prepare(playback_handle_.get()) < 0) {
    ERROR("Cannot prepare playback stream");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Pause() {
  LOG("Pause playback stream");
  if (!playback_handle_) return error::kOpenDeviceFailed;

  if (snd_pcm_drop(playback_handle_.get()) < 0) {
    ERROR("Cannot pause playback stream and clear remaining frames on buffer");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::Stop() {
  LOG("Stop playback stream");
  if (!playback_handle_) return error::kOpenDeviceFailed;

  if (snd_pcm_drain(playback_handle_.get()) < 0) {
    ERROR("Cannot stop playback stream and preserve remaining frames on buffer");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Alsa::AudioCallback(void* buffer, int size) {
  // There is no playback stream when output device could not be opened
  if (!playback_handle_) return error::kPlaybackFailed;

  // As this is called multiple times, LOG will not be called here in the beginning
  auto result = static_cast<int>(snd_pcm_writei(playback_handle_.get(), buffer, size));
  if (result >= 0) return error::kSuccess;

  if (result == -EPIPE) {
    // Underrun: samples were not written in time (e.g. waiting for network), so song stuttered
    WARN("Playback underrun, audio device ran out of samples to play (audible gap)");
  } else {
    ERROR("Cannot write buffer to playback stream, error=", snd_strerror(result));
  }

  // Attempt to recover from error (e.g. overrun/underrun or suspended device)
  if (int recovered = snd_pcm_recover(playback_handle_.get(), result, 1); recovered < 0) {
    ERROR("Cannot recover playback stream, error=", snd_strerror(recovered));
    return error::kPlaybackFailed;
  }

  LOG("Recovered playback stream from error=", snd_strerror(result));

  // Buffer was not written, so try it again
  result = static_cast<int>(snd_pcm_writei(playback_handle_.get(), buffer, size));
  if (result < 0) {
    ERROR("Cannot write buffer to playback stream after recovering, error=", snd_strerror(result));
    return error::kPlaybackFailed;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

snd_mixer_elem_t* Alsa::GetMasterPlayback() {
  LOG("Use mixer to get master playback");

  // Mixer is not available for current device
  if (!mixer_) return nullptr;

  // Select master playback
  snd_mixer_selem_id_t* sid = nullptr;

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_name(sid, kSelemName);

  snd_mixer_elem_t* elem = snd_mixer_find_selem(mixer_.get(), sid);
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
  long min;
  long max;
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
