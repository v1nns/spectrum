/**
 * \file
 * \brief  Class for rendering spectrum visualizer animation picker
 */

#ifndef INCLUDE_VIEW_ELEMENT_ANIMATION_PICKER_H_
#define INCLUDE_VIEW_ELEMENT_ANIMATION_PICKER_H_

#include <cstddef>
#include <functional>

#include "model/bar_animation.h"
#include "view/element/picker.h"

namespace interface {

/**
 * @brief Picker to choose animation from spectrum visualizer, shown over it: animation is changed
 * while selection moves (so user can see it before choosing). As it does not cover the other
 * blocks, events not used by picker are still handled by them
 */
class AnimationPicker : public Picker {
 public:
  using Callback = std::function<void(model::BarAnimation)>;

  /**
   * @brief Construct a new AnimationPicker object
   * @param on_preview Callback to show the given animation (selected one, or the one from before
   * opening picker when it is cancelled)
   * @param on_keep Callback to keep the given animation, as it was chosen
   */
  AnimationPicker(const Callback& on_preview, const Callback& on_keep);

  /**
   * @brief Destroy AnimationPicker object
   */
  ~AnimationPicker() override = default;

  /**
   * @brief Set picker as visible, with the given animation selected
   * @param current Animation in use
   */
  void Open(model::BarAnimation current);

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  //! Show selected animation right away
  void OnSelect(size_t index) override;

  //! Keep selected animation
  void OnChoose(size_t index) override;

  //! Go back to the animation from before opening picker
  void OnCancel() override;

  //! Picker is shown only over visualizer
  bool IsModal() const override { return false; }

  /* ******************************************************************************************** */
  //! Variables

  Callback on_preview_;  //!< Show an animation
  Callback on_keep_;     //!< Keep an animation

  //! Animation from before opening picker (to restore it)
  model::BarAnimation previous_ = model::BarAnimation::HorizontalMirror;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_ANIMATION_PICKER_H_
