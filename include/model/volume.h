/**
 * \file
 * \brief  Base class for sound volume
 */

#ifndef INCLUDE_MODEL_VOLUME_H_
#define INCLUDE_MODEL_VOLUME_H_

#include <math.h>

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

  // Convenient conversion to int
  explicit operator int() const { return !muted ? (int)round(percentage * 100) : 0; }

  // Convenient conversion to float
  explicit operator float() const { return !muted ? percentage : 0.F; }

  // For comparisons
  friend bool operator==(const Volume lhs, const Volume rhs) {
    return lhs.percentage == rhs.percentage;
  }
  friend bool operator!=(const Volume lhs, const Volume rhs) { return !(lhs == rhs); }

  // Output to ostream
  friend std::ostream& operator<<(std::ostream& out, const Volume& v) {
    out << "{volume:" << (int)v << "% ";
    out << "muted: " << (v.muted ? "true" : "false") << "}";
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
  ss << (float)arg;
  return std::move(ss).str();
}

}  // namespace model
#endif  // INCLUDE_MODEL_VOLUME_H_
