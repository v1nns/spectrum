/**
 * \file
 * \brief  Class for rendering question dialog
 */

#ifndef INCLUDE_VIEW_ELEMENT_QUESTION_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_QUESTION_DIALOG_H_

#include <optional>

#include "model/question_data.h"
#include "view/base/dialog.h"
#include "view/element/button.h"

namespace interface {

/**
 * @brief Customized dialog box to show a question message
 */
class QuestionDialog : public Dialog {
  static constexpr int kMaxColumns = 45;  //!< Maximum columns for Element
  static constexpr int kMaxLines = 5;     //!< Maximum lines for Element

 public:
  /**
   * @brief Construct a new QuestionDialog object
   */
  QuestionDialog();

  /**
   * @brief Destroy QuestionDialog object
   */
  virtual ~QuestionDialog() = default;

  /**
   * @brief Set question message to show on dialog
   * @param data Question message with custom callbacks
   */
  void SetMessage(const model::QuestionData& data);

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
  void OnClose() override { content_.reset(); }

  /* ******************************************************************************************** */
  //! Variables

  std::optional<model::QuestionData> content_;  // !< Question content

  GenericButton btn_yes_;  //!< "Yes" button
  GenericButton btn_no_;   //!< "No" button
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_QUESTION_DIALOG_H_
