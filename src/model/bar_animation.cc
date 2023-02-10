#include "model/bar_animation.h"

namespace model {

//! BarAnimation pretty print
std::ostream& operator<<(std::ostream& out, const BarAnimation& animation) {
  switch (animation) {
    case BarAnimation::HorizontalMirror:
      out << "HorizontalMirror";
      break;
    case BarAnimation::VerticalMirror:
      out << "VerticalMirror";
      break;
    case BarAnimation::Mono:
      out << "Mono";
      break;
    case BarAnimation::LAST:
      out << "Invalid";
      break;
  }

  return out;
}

}  // namespace model