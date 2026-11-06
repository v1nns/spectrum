/**
 * \file
 * \brief  Class for rendering spectrum visualizer options picker
 */

#ifndef INCLUDE_VIEW_ELEMENT_VISUALIZER_PICKER_H_
#define INCLUDE_VIEW_ELEMENT_VISUALIZER_PICKER_H_

#include <cstddef>
#include <cstdint>
#include <functional>

#include "model/bar_animation.h"
#include "view/element/picker.h"

namespace interface {

/**
 * @brief Picker to choose how spectrum visualizer looks, shown over it. Each entry is an option
 * (animation, bar style, colors...), whose value is changed with left/right keys or mouse wheel,
 * and visualizer shows it right away (so user can see it before choosing). Options that do not
 * apply to the current animation are disabled. As it does not cover the other blocks, events not
 * used by picker are still handled by them
 */
class VisualizerPicker : public Picker {
  //! Options shown by picker, one for each entry
  enum class Option : uint8_t { Animation, Bars, Spacing, Colors, Peaks, Hue, LAST };

 public:
  using Callback = std::function<void(const model::VisualizerOptions&)>;

  /**
   * @brief Construct a new VisualizerPicker object
   * @param on_preview Callback to show the given options (changed ones, or the ones from before
   * opening picker when it is cancelled)
   * @param on_keep Callback to keep the given options, as they were chosen
   */
  VisualizerPicker(const Callback& on_preview, const Callback& on_keep);

  /**
   * @brief Destroy VisualizerPicker object
   */
  ~VisualizerPicker() override = default;

  /**
   * @brief Set picker as visible, with the given options and the first entry selected
   * @param current Options in use
   */
  void Open(const model::VisualizerOptions& current);

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  //! Update entries, as the selected one shows arrows around its value
  void OnSelect(size_t index) override;

  //! Keep options
  void OnChoose(size_t index) override;

  //! Go back to the options from before opening picker
  void OnCancel() override;

  //! Change value from selected option with left/right keys
  bool OnKey(const ftxui::Event& event) override;

  //! Change value from selected option with mouse wheel
  void HandleWheel(const ftxui::Mouse::Button& button) override;

  //! Picker is shown only over visualizer
  bool IsModal() const override { return false; }

  //! Change value from selected option to the next/previous one (going around), and show it
  void Change(bool next);

  //! Check if option applies to the current options (e.g. bar style does not apply to lines)
  bool IsEnabled(Option option) const;

  //! Build entries from current options
  void UpdateEntries();

  /* ******************************************************************************************** */
  //! Variables
  Callback on_preview_;  //!< Show options
  Callback on_keep_;     //!< Keep options

  model::VisualizerOptions options_;   //!< Options being changed
  model::VisualizerOptions previous_;  //!< Options from before opening picker (to restore them)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_VISUALIZER_PICKER_H_
