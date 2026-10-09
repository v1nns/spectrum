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
    case BarAnimation::SpectrumLine:
      return "SpectrumLine";
    case BarAnimation::SpectrumLineMirror:
      return "SpectrumLineMirror";
    case BarAnimation::SpectrumLineFilled:
      return "SpectrumLineFilled";
    case BarAnimation::SpectrumLineFilledMirror:
      return "SpectrumLineFilledMirror";
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
    case BarAnimation::SpectrumLine:
      return "Line";
    case BarAnimation::SpectrumLineMirror:
      return "Line (mirror)";
    case BarAnimation::SpectrumLineFilled:
      return "Line (filled)";
    case BarAnimation::SpectrumLineFilledMirror:
      return "Line (filled mirror)";
    case BarAnimation::LAST:
    default:
      return "Invalid";
  }
}

/* ********************************************************************************************** */

std::string_view GetAnimationId(const BarAnimation& animation) {
  switch (animation) {
    case BarAnimation::HorizontalMirror:
      return "horizontal-mirror";
    case BarAnimation::VerticalMirror:
      return "vertical-mirror";
    case BarAnimation::Mono:
      return "mono";
    case BarAnimation::HorizontalMirrorNoSpace:
      return "horizontal-mirror-no-space";
    case BarAnimation::VerticalMirrorNoSpace:
      return "vertical-mirror-no-space";
    case BarAnimation::MonoNoSpace:
      return "mono-no-space";
    case BarAnimation::SpectrumLine:
      return "line";
    case BarAnimation::SpectrumLineMirror:
      return "line-mirror";
    case BarAnimation::SpectrumLineFilled:
      return "line-filled";
    case BarAnimation::SpectrumLineFilledMirror:
      return "line-filled-mirror";
    case BarAnimation::LAST:
    default:
      return "invalid";
  }
}

/* ********************************************************************************************** */

std::optional<BarAnimation> GetAnimationFromId(std::string_view id) {
  for (int value = BarAnimation::HorizontalMirror; value < BarAnimation::LAST; value++) {
    const auto animation = static_cast<BarAnimation>(value);
    if (GetAnimationId(animation) == id) return animation;
  }

  return std::nullopt;
}

}  // namespace model
