/**
 * \file
 * \brief Structure for a bar animation
 */

#ifndef INCLUDE_MODEL_BAR_ANIMATION_H_
#define INCLUDE_MODEL_BAR_ANIMATION_H_

#include <iostream>

namespace model {

/**
 * @brief Contains bar animations that can be rendered by spectrum visualizer
 */
enum BarAnimation {
  HorizontalMirror = 11000,         //!< Both channels (L/R) are mirrored horizontally (default)
  VerticalMirror = 11001,           //!< Both channels (L/R) are mirrored vertically
  Mono = 11002,                     //!< Average from the sum of both channels (L/R)
  HorizontalMirrorNoSpace = 11003,  //!< Both channels (L/R) are mirrored horizontally without space
  VerticalMirrorNoSpace = 11004,    //!< Both channels (L/R) are mirrored vertically without space
  MonoNoSpace = 11005,              //!< Average from the sum of both channels (L/R) without space
  LAST = 11006,
};

//! BarAnimation pretty print
std::ostream& operator<<(std::ostream& out, const BarAnimation& animation);

//! Utility method to check if animation has spacing or not
inline bool IsAnimationSpaced(BarAnimation& animation) {
  switch (animation) {
    case HorizontalMirror:
    case VerticalMirror:
    case Mono:
      return true;

    case HorizontalMirrorNoSpace:
    case VerticalMirrorNoSpace:
    case MonoNoSpace:
      return false;

    case LAST:
    default:
      return true;
  }
};

}  // namespace model

#endif  // INCLUDE_MODEL_BAR_ANIMATION_H_
