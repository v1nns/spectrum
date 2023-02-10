/**
 * \file
 * \brief Structure for a bar animation
 */

#ifndef INCLUDE_MODEL_BAR_ANIMATION_H_
#define INCLUDE_MODEL_BAR_ANIMATION_H_

#include <iostream>
#include <string>

namespace model {

/**
 * @brief Contains bar animations that can be rendered by spectrum visualizer
 */
enum BarAnimation {
  HorizontalMirror = 11000,  //!< Both channels (L/R) are mirrored horizontally (default)
  VerticalMirror = 11001,    //!< Both channels (L/R) are mirrored vertically
  Mono = 11002,              //!< Average from the sum of both channels (L/R)
  LAST = 11003,
};

//! BarAnimation pretty print
std::ostream& operator<<(std::ostream& out, const BarAnimation& a);

}  // namespace model

#endif  // INCLUDE_MODEL_BAR_ANIMATION_H_
