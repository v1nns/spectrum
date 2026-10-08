#include "model/audio_filter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
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

bool operator==(const AudioFilter& lhs, const AudioFilter& rhs) {
  return std::tie(lhs.frequency, lhs.Q, lhs.gain) == std::tie(rhs.frequency, rhs.Q, rhs.gain);
}

bool operator!=(const AudioFilter& lhs, const AudioFilter& rhs) { return !(lhs == rhs); }

/* ********************************************************************************************** */

EqualizerPresets AudioFilter::CreatePresets() {
  //! Gain for each frequency from a preset
  using Gains = std::array<double, equalizer::kFiltersPerPreset>;

  //! Frequencies used by every preset
  static constexpr Gains kFrequencies{32, 64, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};

  //! Create preset using the given gains (only the preset for user may have its gains modified)
  auto create = [](const Gains& gains, bool modifiable = false) {
    EqualizerPreset preset;

    for (size_t i = 0; i < preset.size(); i++) {
      preset.at(i) = AudioFilter{
          .frequency = kFrequencies.at(i), .gain = gains.at(i), .modifiable = modifiable};
    }

    return preset;
  };

  return EqualizerPresets{
      {std::string{equalizer::kCustomPreset}, create({}, /*modifiable=*/true)},
      {"Acoustic", create({2, 2, 1, 0, 1, 1, 2, 2, 2, 1})},
      {"Bass Boost", create({4, 3, 2, 1, 0, 0, 0, 0, 0, 0})},
      {"Classical", create({2, 2, 1, 1, -1, -1, 0, 1, 2, 2})},
      {"Dance", create({2, 4, 3, 0, -1, 0, 2, 2, 1, 0})},
      {"Electronic", create({2, 3, 2, -2, 0, 1, 1, 1, 2, 2})},
      {std::string{equalizer::kFlatPreset}, create({})},
      {"Hip-Hop", create({4, 3, 1, 2, -1, -1, 1, 0, 1, 2})},
      {"Jazz", create({3, 2, 1, 2, -2, -2, 0, 1, 2, 3})},
      {"Loudness", create({4, 3, 0, 0, -1, 0, -1, -2, 3, 1})},
      {"Pop", create({-1, -1, 0, 1, 2, 2, 1, 0, -1, -1})},
      {"Rock", create({1, 2, 1, -1, -3, -1, 0, 1, 2, 3})},
      {"Treble Boost", create({0, 0, 0, 0, 0, 0, 1, 2, 3, 4})},
      {"Vocal", create({-2, -2, -1, 1, 2, 2, 2, 1, 0, -1})},
  };
}

/* ********************************************************************************************** */

double AudioFilter::CalculatePeakGain(const std::vector<AudioFilter>& filters, double sample_rate) {
  static constexpr double kPi = 3.14159265358979323846;
  static constexpr double kMinFrequency = 20;     // Lowest frequency to check
  static constexpr double kMaxFrequency = 20000;  // Highest frequency to check
  static constexpr int kPoints = 256;             // Frequencies to check (in a logarithmic scale)

  // Response from a single filter, which is a peaking equalizer (the same one created by decoder)
  auto response = [sample_rate](const AudioFilter& filter, const std::complex<double>& z) {
    const double amplitude = std::pow(10.0, filter.gain / 40.0);
    const double omega = 2.0 * kPi * filter.frequency / sample_rate;
    const double alpha = std::sin(omega) / (2.0 * filter.Q);
    const double cosine = -2.0 * std::cos(omega);

    return ((1.0 + (alpha * amplitude)) + (cosine * z) + ((1.0 - (alpha * amplitude)) * z * z)) /
           ((1.0 + (alpha / amplitude)) + (cosine * z) + ((1.0 - (alpha / amplitude)) * z * z));
  };

  // There is nothing above half of sample rate
  const double max_frequency = std::min(kMaxFrequency, sample_rate / 2.0);
  if (sample_rate <= 0 || max_frequency <= kMinFrequency) return 0;

  const double step = std::pow(max_frequency / kMinFrequency, 1.0 / (kPoints - 1));
  double frequency = kMinFrequency;
  double peak = 1.0;

  for (int i = 0; i < kPoints; i++, frequency *= step) {
    const std::complex<double> z =
        std::exp(std::complex<double>(0.0, -2.0 * kPi * frequency / sample_rate));

    // Filters are used one after the other, so their responses are multiplied
    std::complex<double> total{1.0, 0.0};

    for (const auto& filter : filters) {
      // Filter for half of sample rate (or above it) does not change anything
      if (filter.gain == 0 || filter.Q <= 0 || filter.frequency >= sample_rate / 2.0) continue;

      total *= response(filter, z);
    }

    peak = std::max(peak, std::abs(total));
  }

  return 20.0 * std::log10(peak);
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
