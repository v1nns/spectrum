/**
 * \file
 * \brief Header for UI utils
 */

#ifndef INCLUDE_VIEW_ELEMENT_UTIL_H_
#define INCLUDE_VIEW_ELEMENT_UTIL_H_

namespace interface {

//! Similar to std::clamp, but allow hi to be lower than lo.
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_UTIL_H_
