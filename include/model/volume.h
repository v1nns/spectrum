/**
 * \file
 * \brief  Base class for sound volume
 */

#ifndef INCLUDE_MODEL_VOLUME_H_
#define INCLUDE_MODEL_VOLUME_H_

#include <math.h>

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

namespace model {

/**
 * @brief General sound volume
 */
struct Volume {
 public:
  //! Default constructor
  Volume() : percentage{1.f} {};

  explicit Volume(float value) : percentage{std::min(std::max(value, 0.f), 1.f)} {}

  // Pre-increment
  Volume& operator++() {
    percentage += 0.05f;
    if (percentage > 1.f) percentage = 1.f;
    return *this;
  }

  // Post-increment
  Volume operator++(int) {
    Volume tmp{*this};  // create temporary with old value
    operator++();       // perform increment
    return tmp;         // return temporary
  }

  // Pre-decrement
  Volume& operator--() {
    percentage -= 0.05f;
    if (percentage < 0.0f) percentage = 0.0f;
    return *this;
  }

  // Post-decrement
  Volume operator--(int) {
    Volume tmp{*this};  // create temporary with old value
    operator--();       // perform decrement
    return tmp;         // return temporary
  }

  // Toggle volume mute
  void ToggleMute() { muted = !muted; }

  // Get mute state
  bool IsMuted() const { return muted; }

  // Get volume level, even when muted (from 0.f to 1.f)
  float GetLevel() const { return percentage; }

  // Convenient conversion to int
  explicit operator int() const { return !muted ? static_cast<int>(round(percentage * 100)) : 0; }

  // Convenient conversion to float
  explicit operator float() const { return !muted ? percentage : 0.f; }

  // For comparisons
  friend bool operator==(const Volume lhs, const Volume rhs) {
    return lhs.percentage == rhs.percentage;
  }
  friend bool operator!=(const Volume lhs, const Volume rhs) { return !(lhs == rhs); }

  // Output to ostream
  friend std::ostream& operator<<(std::ostream& out, const Volume& v) {
    out << "{volume:\"" << static_cast<int>(v) << "%\"";
    out << ", muted:" << (v.muted ? "true" : "false") << "}";
    return out;
  }

 private:
  float percentage;    //!< Volume percentage
  bool muted = false;  //!< Control flag to mute/unmute volume
};

/**
 * @brief Util method to pretty print Volume structure
 * @param arg Volume struct
 * @return std::string Formatted string with properties from Volume
 */
inline std::string to_string(const Volume& arg) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(2) << static_cast<float>(arg);
  return std::move(ss).str();
}

//! Level (in decibels) reduced from any volume, so maximum volume is not the full scale
static constexpr float kVolumeReference = 6.0F;

//! Decibels reduced each time that volume level is divided by ten. As amplitude follows the cube of
//! volume level (the same curve used by sound servers, like PipeWire and PulseAudio), steps in the
//! volume level are perceived as steps of about the same size in loudness
static constexpr float kVolumeDecibelsPerDecade = 60.0F;

/**
 * @brief Convert Volume to decibel scale for FFmpeg
 * @param arg Volume struct
 * @param gain Gain (in decibels) added to volume, which must not be higher than kVolumeReference
 * @return std::string Volume in decibels (e.g., "-20.00dB")
 */
inline std::string to_string_db(const Volume& arg, float gain = 0.0F) {
  float linear = static_cast<float>(arg);
  if (linear <= 0.0f) {
    return "-100dB";  // Effectively silent
  }

  // Apply -6dB reference adjustment to match streaming service loudness normalization
  // This compensates for hot-mastered tracks and provides more reasonable volume range
  float db = kVolumeDecibelsPerDecade * log10f(linear) - kVolumeReference + gain;

  std::ostringstream ss;
  ss << std::fixed << std::setprecision(2) << db << "dB";
  return std::move(ss).str();
}

}  // namespace model
#endif  // INCLUDE_MODEL_VOLUME_H_
