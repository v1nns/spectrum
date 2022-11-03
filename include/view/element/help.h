/**
 * \file
 * \brief  Class for rendering a customized menu helper
 */

#ifndef INCLUDE_VIEW_ELEMENT_HELP_H_
#define INCLUDE_VIEW_ELEMENT_HELP_H_

#include <memory>

#include "ftxui/component/event.hpp"  // for Event
#include "ftxui/dom/elements.hpp"     // for Element
#include "ftxui/screen/terminal.hpp"  // for ScreenInteractive

namespace interface {

/**
 * @brief Customized dialog box to show a helper
 */
class Help {
  static constexpr int kMaxColumns = 90;  //!< Maximum columns for Element
  static constexpr int kMaxLines = 25;    //!< Maximum lines for Element

 public:
  /**
   * @brief Construct a new Help object
   */
  Help();

  /**
   * @brief Destroy Help object
   */
  virtual ~Help() = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render();

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event);

  /**
   * @brief Set dialog state to visible
   */
  void Show();

  /**
   * @brief Reset dialog state to initial value
   */
  void Clear();

  /**
   * @brief Indicates if dialog is visible
   *
   * @return true if dialog visible, otherwise false
   */
  bool IsVisible() { return opened_; }

  /* ******************************************************************************************** */
 private:
  //! Style for each part of the dialog
  struct DialogStyle {
    ftxui::Color background;
    ftxui::Color foreground;
  };

  DialogStyle style_;    //!< Color style
  bool opened_;          //!< Flag to indicate dialog visilibity
  std::string message_;  //!< Custom error message
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_HELP_H_
