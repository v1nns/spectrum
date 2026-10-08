/**
 * \file
 * \brief  Class for rendering theme picker
 */

#ifndef INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_
#define INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_

#include <cstddef>
#include <memory>

#include "util/file_handler.h"
#include "view/element/picker.h"

namespace interface {

/**
 * @brief Picker to choose UI theme: theme is applied while selection moves (so user can see it
 * before choosing) and saved to be restored on next run
 */
class ThemePicker : public Picker {
 public:
  /**
   * @brief Construct a new ThemePicker object, applying theme saved in settings (or the default
   * one, when there is none or it is unknown)
   * @param file_handler Utility handler to load/save theme in settings
   */
  explicit ThemePicker(const std::shared_ptr<util::FileHandler>& file_handler);

  /**
   * @brief Destroy ThemePicker object
   */
  ~ThemePicker() override = default;

  /**
   * @brief Set picker as visible, with current theme selected
   */
  void Open();

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  //! Replace theme used by UI with the one from the given index in list of themes
  void Apply(size_t index) const;

  //! Apply selected theme right away (so user can see it while choosing)
  void OnSelect(size_t index) override;

  //! Keep selected theme, saving it to be restored on next run
  void OnChoose(size_t index) override;

  //! Go back to the theme from before opening picker
  void OnCancel() override;

  /* ******************************************************************************************** */
  //! Variables

  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save theme in settings

  size_t previous_ = 0;  //!< Index of theme from before opening picker (to restore it)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_
