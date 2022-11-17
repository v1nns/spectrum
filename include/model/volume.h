/**
 * \file
 * \brief  Base class for sound volume
 */

#ifndef INCLUDE_MODEL_VOLUME_H_
#define INCLUDE_MODEL_VOLUME_H_

#include <math.h>

#include <cstdio>
#include <fstream>
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

  Volume(float value) : percentage{std::min(std::max(value, 0.f), 1.f)} {}

  // pre-increment
  Volume& operator++() {
    percentage += 0.05f;
    if (percentage > 1.f) percentage = 1.f;
    return *this;
  }

  // post-increment
  Volume operator++(int) {
    Volume tmp{*this};  // create temporary with old value
    operator++();       // perform increment
    return tmp;         // return temporary
  }

  // pre-decrement
  Volume& operator--() {
    percentage -= 0.05f;
    if (percentage < 0.0f) percentage = 0.0f;
    return *this;
  }

  // post-decrement
  Volume operator--(int) {
    Volume tmp{*this};  // create temporary with old value
    operator--();       // perform decrement
    return tmp;         // return temporary
  }

  // TODO: maybe extend to implement the following operators
  // Volume operator+(const Volume& x);
  // Volume operator+(int x);
  // Volume operator-(const Volume& x);
  // Volume operator-(int x);
  // Volume& operator+=(const Volume& x);
  // Volume& operator+=(int x);
  // Volume& operator-=(const Volume& x);
  // Volume& operator-=(int x);
  // and lots more, for *, /, unary +/-, etc...

  // convenient conversion to int
  explicit operator int() { return (int)round(percentage * 100); }

  // convenient conversion to float
  explicit operator float() { return percentage; }

  // for comparisons
  bool operator==(const Volume other) const { return percentage == other.percentage; }
  bool operator!=(const Volume other) const { return !operator==(other); }

  // output to ostream
  friend std::ostream& operator<<(std::ostream& out, const Volume& v) {
    out << "{volume:" << int(Volume{v}) << "%}";
    return out;
  }

 private:
  float percentage;
};

}  // namespace model
#endif  // INCLUDE_MODEL_VOLUME_H_
