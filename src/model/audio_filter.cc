#include "model/audio_filter.h"

#include <math.h>

#include <string>
#include <tuple>

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

std::string AudioFilter::ToString() {
  std::ostringstream ss;
  ss << "freq_" << frequency;
  return std::move(ss).str();
}

};  // namespace model