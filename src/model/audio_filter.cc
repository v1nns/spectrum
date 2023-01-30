#include "model/audio_filter.h"

#include <math.h>

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
  ss << " " << util::to_string_with_precision(gain, 0) << " dB ";
  return std::move(ss).str();
}

/* ********************************************************************************************** */

float AudioFilter::GetGainAsPercentage() const {
  return float(gain - model::AudioFilter::kMinGain) /
         float(model::AudioFilter::kMaxGain - model::AudioFilter::kMinGain);
}

};  // namespace model