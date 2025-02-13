#include "model/bar_animation.h"

#include <iomanip>

namespace model {

static const char* to_chars(const BarAnimation& animation) {
  switch (animation) {
    case BarAnimation::HorizontalMirror:
      return "HorizontalMirror";
    case BarAnimation::VerticalMirror:
      return "VerticalMirror";
    case BarAnimation::Mono:
      return "Mono";
    case BarAnimation::HorizontalMirrorNoSpace:
      return "HorizontalMirrorNoSpace";
    case BarAnimation::VerticalMirrorNoSpace:
      return "VerticalMirrorNoSpace";
    case BarAnimation::MonoNoSpace:
      return "MonoNoSpace";
    case BarAnimation::LAST:
    default:
      return "Invalid";
  }
}

/* ********************************************************************************************** */

//! BarAnimation pretty print
std::ostream& operator<<(std::ostream& out, const BarAnimation& animation) {
  out << std::quoted(to_chars(animation));
  return out;
}

}  // namespace model
