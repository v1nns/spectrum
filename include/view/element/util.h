/**
 * \file
 * \brief Header for UI utils
 */

#ifndef INCLUDE_VIEW_ELEMENT_UTIL_H_
#define INCLUDE_VIEW_ELEMENT_UTIL_H_

#include <ftxui/dom/elements.hpp>

namespace interface {

//! Similar to std::clamp, but allow hi to be lower than lo.
template <class T>
inline constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

//! Returns a decorator that sets the size of an element to the specified width and height.
inline const ftxui::Decorator set_size(int width, int height) {
  return ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width) |
         ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, height);
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_UTIL_H_
