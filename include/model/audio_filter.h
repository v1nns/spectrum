/**
 * \file
 * \brief  Base class for an audio filter
 */

#ifndef INCLUDE_MODEL_AUDIO_FILTER_H_
#define INCLUDE_MODEL_AUDIO_FILTER_H_

#include <array>
#include <cstdio>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace model {

/**
 * @brief Class representing an audio filter, more specifically, a Biquad filter. It is a type of
 * digital filter that is widely used in audio processing applications. It is a second-order filter,
 * meaning it has two poles and two zeroes in its transfer function. This allows it to have a more
 * complex response than a first-order filter.
 */
struct AudioFilter {
  //! Constants
  static constexpr double kMinGain = -12;  //!< Minimum value of gain
  static constexpr double kMaxGain = 12;   //!< Maximum value of gain
  static constexpr int kPresetSize = 10;   //!< Maximum number of filters for each preset

  //! Overloaded operators
  friend std::ostream& operator<<(std::ostream& out, const AudioFilter& a);
  bool operator==(const AudioFilter& other) const;
  bool operator!=(const AudioFilter& other) const;

  /* ******************************************************************************************** */
  //! Utilities

  /**
   * @brief Create default vector containing 10 audio filters to use it on GUI
   * @return Array containing audio filters
   */
  static EqualizerPreset CreateCustomPreset();

  /**
   * @brief Create a map of presets to use it on GUI
   * @return Map of EQ presets
   */
  static EqualizerPresets CreateGenrePresets();

  /**
   * @brief Get audio filter name based on cutoff frequency
   * @return A string containing filter name using the pattern "freq_123"
   */
  std::string GetName() const;

  /**
   * @brief Get cutoff frequency of filter
   * @return A string containing cutoff frequency
   */
  std::string GetFrequency() const;

  /**
   * @brief Get filter gain of filter
   * @return A string containing filter gain
   */
  std::string GetGain() const;

  /**
   * @brief Get gain as a percentage considering the min and max values for gain
   * @return A percentage value of gain (0~1)
   */
  float GetGainAsPercentage() const;

  /**
   * @brief Set new value for gain and normalize it based on the range between minimum and maximum
   * @param value New value for gain
   */
  void SetNormalizedGain(double value);

  /* ******************************************************************************************** */
  //! Variables

  double frequency;  //!< Cutoff frequency or center frequency, it is the frequency at which the
                     //!< filter's response is half the maximum value (measured in Hertz)
  double Q = 1.41;   //!< Ratio of center frequency to the width of the passband
  double gain = 0;   //!< Measure of how much the amplitude of the output signal is increased or
                     //!< decreased relative to the input signal. It is defined as the ratio of the
                     //!< output signal's amplitude to the input signal's amplitude.
};

// TODO: doc
using MusicGenre = std::string;
using EqualizerPreset = std::array<AudioFilter, AudioFilter::kPresetSize>;
using EqualizerPresets = std::unordered_map<MusicGenre, EqualizerPreset>;

}  // namespace model
#endif  // INCLUDE_MODEL_AUDIO_FILTER_H_
