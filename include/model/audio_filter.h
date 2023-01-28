/**
 * \file
 * \brief  Base class for an audio filter
 */

#ifndef INCLUDE_MODEL_AUDIO_FILTER_H_
#define INCLUDE_MODEL_AUDIO_FILTER_H_

#include <cstdio>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>

namespace model {

/**
 * @brief Class representing an audio filter, more specifically, a Biquad filter. It is a type of
 * digital filter that is widely used in audio processing applications. It is a second-order filter,
 * meaning it has two poles and two zeroes in its transfer function. This allows it to have a more
 * complex response than a first-order filter.
 */
struct AudioFilter {
  double frequency;  //!< Cutoff frequency or center frequency, it is the frequency at which the
                     //!< filter's response is half the maximum value (measured in Hertz)
  double Q;          //!< Ratio of center frequency to the width of the passband
  double gain;       //!< Measure of how much the amplitude of the output signal is increased or
                     //!< decreased relative to the input signal. It is defined as the ratio of the
                     //!< output signal's amplitude to the input signal's amplitude.

  //! Overloaded operators
  friend std::ostream& operator<<(std::ostream& out, const AudioFilter& a);
  bool operator==(const AudioFilter& other) const;
  bool operator!=(const AudioFilter& other) const;

  /**
   * @brief Get unique audio filter name based on cutoff frequency
   * @return A string containing filter name using the pattern "freq_123"
   */
  std::string ToString();
};

}  // namespace model
#endif  // INCLUDE_MODEL_AUDIO_FILTER_H_
