/**
 * \file
 * \brief  Class for rendering error dialog
 */

#ifndef INCLUDE_VIEW_ELEMENT_ERROR_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_ERROR_DIALOG_H_

#include <string>
#include <string_view>

#include "view/base/dialog.h"

namespace interface {

/**
 * @brief Customized dialog box to show messages (error/warning/info)
 */
class ErrorDialog : public Dialog {
  static constexpr int kMaxColumns = 35;  //!< Maximum columns for Element
  static constexpr int kMaxLines = 5;     //!< Maximum lines for Element

 public:
  /**
   * @brief Construct a new ErrorDialog object
   */
  ErrorDialog();

  /**
   * @brief Destroy ErrorDialog object
   */
  ~ErrorDialog() override = default;

  /**
   * @brief Set error message to show on dialog
   * @param message Error message
   */
  void SetErrorMessage(const std::string_view& message);

  /* ******************************************************************************************** */
  //! Custom implementation
 private:
  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element RenderImpl(const ftxui::Dimensions& curr_size) const override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEventImpl(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEventImpl(ftxui::Event event) override;

  /**
   * @brief Callback to notify when dialog is closed
   */
  void OnClose() override { message_.clear(); }

  /* ******************************************************************************************** */
  //! Variables

  std::string message_;  //!< Custom error message
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_ERROR_DIALOG_H_
