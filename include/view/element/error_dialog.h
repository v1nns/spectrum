/**
 * \file
 * \brief  Class for rendering a customized dialog for errors
 */

#ifndef INCLUDE_VIEW_ELEMENT_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_DIALOG_H_

#include <memory>
#include <string_view>

#include "ftxui/component/event.hpp"  // for Event
#include "ftxui/dom/elements.hpp"     // for Element

namespace interface {

/**
 * @brief Customized dialog box to show messages (error/warning/info)
 */
class ErrorDialog {
  static constexpr int kMaxColumns = 35;  //!< Maximum columns for Element
  static constexpr int kMaxLines = 5;     //!< Maximum lines for Element

 public:
  /**
   * @brief Construct a new ErrorDialog object
   */
  ErrorDialog() = default;

  /**
   * @brief Destroy ErrorDialog object
   */
  virtual ~ErrorDialog() = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() const;

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event);

  /**
   * @brief Set error message to show on dialog
   *
   * @param message Error message
   */
  void SetErrorMessage(const std::string_view& message);

  /**
   * @brief Reset dialog state to initial value
   */
  void Clear();

  /**
   * @brief Indicates if dialog is visible
   *
   * @return true if dialog visible, otherwise false
   */
  bool IsVisible() const { return opened_; }

  /* ******************************************************************************************** */
 private:
  //! Style for each part of the dialog
  struct DialogStyle {
    ftxui::Color background;
    ftxui::Color foreground;
  };

  DialogStyle style_ = DialogStyle{.background = ftxui::Color::DarkRedBis,
                                   .foreground = ftxui::Color::Grey93};  //!< Color style
  bool opened_ = false;  //!< Flag to indicate dialog visilibity
  std::string message_;  //!< Custom error message
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_DIALOG_H_
