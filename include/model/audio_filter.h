/**
 * \file
 * \brief  Base class for an audio filter
 */

#ifndef INCLUDE_MODEL_AUDIO_FILTER_H_
#define INCLUDE_MODEL_AUDIO_FILTER_H_

#include <array>
#include <functional>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace model {

namespace equalizer {
static constexpr int kFiltersPerPreset = 10;  //!< Maximum number of audio filters for each preset

static constexpr std::string_view kCustomPreset = "Custom";  //!< Preset modified by user
static constexpr std::string_view kFlatPreset = "Flat";      //!< Preset without any gain

/**
 * @brief Order for presets: the one modified by user and the one without any gain come first (as
 * they are not related to any kind of music), then all the others ordered by name
 */
struct PresetOrder {
  using is_transparent = void;  //!< To search for a preset without creating a string

  bool operator()(std::string_view lhs, std::string_view rhs) const {
    return std::make_pair(GetGroup(lhs), lhs) < std::make_pair(GetGroup(rhs), rhs);
  }

 private:
  //! Presets from a group are always placed before the ones from the next group
  static constexpr int GetGroup(std::string_view name) {
    return name == kCustomPreset ? 0 : name == kFlatPreset ? 1 : 2;
  }
};
}  // namespace equalizer

// Forward declaration
struct AudioFilter;

//! Music-genre name
using MusicGenre = std::string;

//! Single EQ preset
using EqualizerPreset = std::array<AudioFilter, equalizer::kFiltersPerPreset>;

//! Map of EQ presets where key is music genre, and value is an EQ preset
using EqualizerPresets = std::map<MusicGenre, EqualizerPreset, equalizer::PresetOrder>;

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

  //! Overloaded operators
  friend std::ostream& operator<<(std::ostream& out, const AudioFilter& a);
  friend bool operator==(const AudioFilter& lhs, const AudioFilter& rhs);
  friend bool operator!=(const AudioFilter& lhs, const AudioFilter& rhs);

  /* ******************************************************************************************** */
  //! Utilities

  /**
   * @brief Create a map of presets to use it on GUI
   * @return Map of EQ presets
   */
  static EqualizerPresets CreatePresets();

  /**
   * @brief Calculate the highest gain applied to any frequency when the given filters are used
   * together (as filters for nearby frequencies add up, it may be higher than the gain from any
   * of them)
   * @param filters Audio filters
   * @param sample_rate Sample rate from audio to be filtered
   * @return Gain in decibels (never lower than zero, which means that nothing is amplified)
   */
  static double CalculatePeakGain(const std::vector<AudioFilter>& filters, double sample_rate);

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

  bool modifiable = false;  //!< Control if gain can be modified
};

}  // namespace model
#endif  // INCLUDE_MODEL_AUDIO_FILTER_H_
