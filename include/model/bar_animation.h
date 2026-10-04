/**
 * \file
 * \brief Structure for a bar animation
 */

#ifndef INCLUDE_MODEL_BAR_ANIMATION_H_
#define INCLUDE_MODEL_BAR_ANIMATION_H_

#include <iostream>
#include <string_view>

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
  SpectrumLine = 11006,             //!< Line connecting the average of both channels (L/R)
  SpectrumLineMirror = 11007,       //!< Lines from both channels (L/R) mirrored vertically
  SpectrumLineFilled = 11008,       //!< Same as SpectrumLine, but filling the area below the line
  SpectrumLineFilledMirror = 11009,  //!< Same as SpectrumLineMirror, but filling the area of lines
  LAST = 11010,
};

//! BarAnimation pretty print
std::ostream& operator<<(std::ostream& out, const BarAnimation& animation);

/**
 * @brief Get user-friendly name for animation (to be shown on UI)
 * @param animation Bar animation
 * @return Animation name
 */
std::string_view GetAnimationName(const BarAnimation& animation);

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
    case SpectrumLine:
    case SpectrumLineMirror:
    case SpectrumLineFilled:
    case SpectrumLineFilledMirror:
      return false;

    case LAST:
    default:
      return true;
  }
};

/**
 * @brief Check if animation draws each channel (or their average) across the whole width, instead
 * of splitting width between both channels. In this case, it needs twice the number of bars that
 * fit on screen from audio analysis
 * @param animation Bar animation
 * @return true if animation uses the whole width for each channel, otherwise false
 */
inline bool IsAnimationFullWidthPerChannel(const BarAnimation& animation) {
  switch (animation) {
    case VerticalMirror:
    case Mono:
    case VerticalMirrorNoSpace:
    case MonoNoSpace:
    case SpectrumLine:
    case SpectrumLineMirror:
    case SpectrumLineFilled:
    case SpectrumLineFilledMirror:
      return true;

    case HorizontalMirror:
    case HorizontalMirrorNoSpace:
    case LAST:
    default:
      return false;
  }
}

}  // namespace model

#endif  // INCLUDE_MODEL_BAR_ANIMATION_H_
