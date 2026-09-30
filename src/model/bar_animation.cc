#include "model/bar_animation.h"

#include <iomanip>
#include <string_view>

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

/* ********************************************************************************************** */

std::string_view GetAnimationName(const BarAnimation& animation) {
  switch (animation) {
    case BarAnimation::HorizontalMirror:
      return "Horizontal mirror";
    case BarAnimation::VerticalMirror:
      return "Vertical mirror";
    case BarAnimation::Mono:
      return "Mono";
    case BarAnimation::HorizontalMirrorNoSpace:
      return "Horizontal mirror (no space)";
    case BarAnimation::VerticalMirrorNoSpace:
      return "Vertical mirror (no space)";
    case BarAnimation::MonoNoSpace:
      return "Mono (no space)";
    case BarAnimation::LAST:
    default:
      return "Invalid";
  }
}

}  // namespace model
