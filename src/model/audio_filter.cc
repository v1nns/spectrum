#include "model/audio_filter.h"

#include <math.h>

#include <algorithm>
#include <string>
#include <tuple>

#include "util/formatter.h"

namespace model {

static constexpr double kSampleRate = 44100;

/* ********************************************************************************************** */

std::ostream& operator<<(std::ostream& out, const AudioFilter& a) {
  out << "{frequency:" << a.frequency << "Q:" << a.Q << " gain:" << a.gain << "}";
  return out;
}

bool AudioFilter::operator==(const AudioFilter& other) const {
  return std::tie(frequency, Q, gain) == std::tie(other.frequency, other.Q, other.gain);
}

bool AudioFilter::operator!=(const AudioFilter& other) const { return !operator==(other); }

/* ********************************************************************************************** */

std::vector<AudioFilter> AudioFilter::Create() {
  return std::vector<AudioFilter>{
      AudioFilter{.frequency = 32},   AudioFilter{.frequency = 64},
      AudioFilter{.frequency = 125},  AudioFilter{.frequency = 250},
      AudioFilter{.frequency = 500},  AudioFilter{.frequency = 1000},
      AudioFilter{.frequency = 2000}, AudioFilter{.frequency = 4000},
      AudioFilter{.frequency = 8000}, AudioFilter{.frequency = 16000},
  };
}

/* ********************************************************************************************** */

std::string AudioFilter::GetName() const {
  std::ostringstream ss;
  ss << "freq_" << frequency;
  return std::move(ss).str();
}

/* ********************************************************************************************** */

std::string AudioFilter::GetFrequency() const { return util::format_with_prefix(frequency, "Hz"); }

/* ********************************************************************************************** */

std::string AudioFilter::GetGain() const {
  std::ostringstream ss;

  std::string gain_str{util::to_string_with_precision(gain, 0)};

  // Maximum length for output string to GUI
  int max_length = gain < 0 ? 6 : 7;

  // Create a dummy margin
  std::string spaces((max_length - gain_str.length()) / 2, ' ');

  ss << spaces << gain_str << " dB" << spaces;
  return std::move(ss).str();
}

/* ********************************************************************************************** */

float AudioFilter::GetGainAsPercentage() const {
  float value = float(gain - kMinGain) / float(kMaxGain - kMinGain);
  // in case of gain equals to zero, return a small value for GUI aesthetics
  return value > 0 ? value : 0.001f;
}

/* ********************************************************************************************** */

void AudioFilter::SetNormalizedGain(double value) {
  gain = std::max(kMinGain, std::min(kMaxGain, value));
}

};  // namespace model