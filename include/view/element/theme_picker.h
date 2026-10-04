/**
 * \file
 * \brief  Class for rendering theme picker
 */

#ifndef INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_
#define INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_

#include <cstddef>
#include <memory>
#include <optional>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/file_handler.h"

namespace interface {

/**
 * @brief Picker to choose UI theme, shown over all blocks: theme is applied while selection moves
 * (so user can see it before choosing) and saved to be restored on next run
 */
class ThemePicker {
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
  virtual ~ThemePicker() = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() const;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event);

  /**
   * @brief Indicates if picker is visible
   * @return true if picker is visible, otherwise false
   */
  bool IsVisible() const { return previous_.has_value(); }

  /**
   * @brief Set picker as visible, with current theme selected
   */
  void Open();

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  //! Replace theme used by UI with the one from the given index in list of themes
  void Apply(size_t index);

  //! Save current theme, so it is restored on next run
  void SaveSettings() const;

  /* ******************************************************************************************** */
  //! Variables

  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save theme in settings

  size_t selected_ = 0;  //!< Index of theme in use (from list of themes)

  //! While picker is open, it contains the theme from before opening it (to restore it)
  std::optional<size_t> previous_;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_
