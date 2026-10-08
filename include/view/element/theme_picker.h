/**
 * \file
 * \brief  Class for rendering theme picker
 */

#ifndef INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_
#define INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/box.hpp"
#include "util/file_handler.h"
#include "view/base/element.h"

namespace interface {

/**
 * @brief Picker to choose UI theme, shown over all blocks: theme is applied while selection moves
 * (so user can see it before choosing) and saved to be restored on next run. Besides keyboard,
 * mouse may be used: wheel moves selection, click selects a theme and double-click keeps it
 */
class ThemePicker : public Element {
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
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) override;

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

  //! Select next/previous theme from list (if any), applying it
  void Move(bool next);

  //! Close picker, keeping selected theme
  void Keep();

  //! Save current theme, so it is restored on next run
  void SaveSettings() const;

  //! Get index of theme rendered at the position of mouse cursor (if any)
  std::optional<size_t> GetEntryAt(const ftxui::Mouse& mouse) const;

  /* ******************************************************************************************** */
  //! Mouse handling (called by Element, only when mouse cursor is over picker)

  //! Move selection
  void HandleWheel(const ftxui::Mouse::Button& button) override;

  //! Select theme under mouse cursor
  void HandleClick(ftxui::Event& event) override;

  //! Select theme under mouse cursor and keep it
  void HandleDoubleClick(ftxui::Event& event) override;

  /* ******************************************************************************************** */
  //! Variables

  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save theme in settings

  size_t selected_ = 0;  //!< Index of theme in use (from list of themes)

  //! While picker is open, it contains the theme from before opening it (to restore it)
  std::optional<size_t> previous_;

  std::vector<ftxui::Box> boxes_;  //!< Single box for each theme rendered (to handle mouse)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_THEME_PICKER_H_
