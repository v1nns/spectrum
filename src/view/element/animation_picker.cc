#include "view/element/animation_picker.h"

#include <string>
#include <utility>
#include <vector>

#include "view/base/keybinding.h"

namespace interface {

namespace {

//! First animation is the one from the first entry
constexpr int kFirstAnimation = model::BarAnimation::HorizontalMirror;

//! Get animation from the given entry
model::BarAnimation ToAnimation(size_t index) {
  return static_cast<model::BarAnimation>(kFirstAnimation + static_cast<int>(index));
}

//! Get entry from the given animation
size_t ToIndex(model::BarAnimation animation) {
  return static_cast<size_t>(animation - kFirstAnimation);
}

}  // namespace

/* ********************************************************************************************** */

AnimationPicker::AnimationPicker(const Callback& on_preview, const Callback& on_keep)
    : Picker("animation", keybinding::Visualizer::ChangeAnimation),
      on_preview_{on_preview},
      on_keep_{on_keep} {
  std::vector<Entry> entries;

  for (int i = kFirstAnimation; i < model::BarAnimation::LAST; i++) {
    const auto animation = static_cast<model::BarAnimation>(i);
    entries.push_back(Entry{.name = std::string{model::GetAnimationName(animation)}});
  }

  SetEntries(std::move(entries));
}

/* ********************************************************************************************** */

void AnimationPicker::Open(model::BarAnimation current) {
  previous_ = current;

  SetSelected(ToIndex(current));
  Show();
}

/* ********************************************************************************************** */

void AnimationPicker::OnSelect(size_t index) { on_preview_(ToAnimation(index)); }

/* ********************************************************************************************** */

void AnimationPicker::OnChoose(size_t index) { on_keep_(ToAnimation(index)); }

/* ********************************************************************************************** */

void AnimationPicker::OnCancel() {
  SetSelected(ToIndex(previous_));
  on_preview_(previous_);
}

}  // namespace interface
