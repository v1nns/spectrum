/**
 * \file
 * \brief  Class for rendering customized dialogs
 */

#ifndef INCLUDE_VIEW_ELEMENT_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_DIALOG_H_

#include "ftxui/component/event.hpp"  // for Event
#include "ftxui/dom/elements.hpp"     // for Element

namespace interface {

class Dialog {
 protected:
  //! Style for each part of the dialog
  struct DialogStyle {
    ftxui::Color background;
    ftxui::Color foreground;
  };

  /**
   * @brief Construct a new Dialog object
   * @param max_columns Maximum number of columns for this dialog instance
   * @param max_lines Maximum number of lines for this dialog instance
   * @param style Dialog style to apply
   */
  Dialog(int max_columns, int max_lines, DialogStyle style);

 public:
  /**
   * @brief Destroy Dialog object
   */
  virtual ~Dialog() = default;

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
   * @brief Indicates if dialog is visible
   * @return true if dialog visible, otherwise false
   */
  bool IsVisible() const { return opened_; }

  /**
   * @brief Set dialog as visible
   */
  void Open() {
    opened_ = true;
    OnOpen();
  }

  /**
   * @brief Set dialog as not visible
   */
  void Close() {
    OnClose();
    opened_ = false;
  }

  /* ******************************************************************************************** */
  //! Implemented by derived class (mandatory)
 private:
  /**
   * @brief Renders the component (implement by derived)
   * @return Element Built element based on internal state
   */
  virtual ftxui::Element RenderImpl() const = 0;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  virtual bool OnEventImpl(const ftxui::Event& event) = 0;

  /* ******************************************************************************************** */
  //! Implemented by derived class (optional)

  /**
   * @brief Callback to notify when dialog is opened
   */
  virtual void OnOpen() {}

  /**
   * @brief Callback to notify when dialog is closed
   */
  virtual void OnClose() {}

  /* ******************************************************************************************** */
  //! Variables

  bool opened_ = false;    //!< Flag to indicate dialog visilibity
  const int max_columns_;  //!< Maximum number of columns
  const int max_lines_;    //!< Maximum number of lines
  DialogStyle style_;      //!< Color style
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_DIALOG_H_
