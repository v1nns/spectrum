/**
 * \file
 * \brief  Class for rendering customized dialogs
 */

#ifndef INCLUDE_VIEW_ELEMENT_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_DIALOG_H_

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace interface {

class Dialog {
  static constexpr int kBorderSize = 2;  //!< Extra padding based on border size

 protected:
  struct Size {
    float width = 0.f;   //!< Width percentage
    float height = 0.f;  //!< Height percentage

    int min_column = 0;  //!< Minimum value of columns
    int min_line = 0;    //!< Minimum value of lines

    int max_column = 0;  //!< Maximum value of columns
    int max_line = 0;    //!< Maximum value of lines
  };

  //! Style for each part of the dialog
  struct Style {
    ftxui::Color background;
    ftxui::Color foreground;
  };

  /**
   * @brief Construct a new Dialog object
   * @param size Size settings for dialog
   * @param style Dialog style to apply
   */
  Dialog(const Size& size, const Style& style);

 public:
  /**
   * @brief Destroy Dialog object
   */
  virtual ~Dialog() = default;

  /**
   * @brief Renders the component
   * @param curr_size Current terminal size
   * @return Element Built element based on internal state
   */
  ftxui::Element Render(const ftxui::Dimensions& curr_size) const;

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
    OnOpen();
    opened_ = true;
  }

  /**
   * @brief Set dialog as not visible
   */
  void Close() {
    opened_ = false;
    OnClose();
  }

  /* ******************************************************************************************** */
  //! Implemented by derived class (mandatory)
 private:
  /**
   * @brief Renders the component (implement by derived)
   * @return Element Built element based on internal state
   */
  virtual ftxui::Element RenderImpl(const ftxui::Dimensions& curr_size) const = 0;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  virtual bool OnEventImpl(const ftxui::Event& event) = 0;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  virtual bool OnMouseEventImpl(ftxui::Event event) = 0;

  /* ******************************************************************************************** */
  //! Implemented by derived class (optional)

  /**
   * @brief Callback to notify when dialog is opened
   */
  virtual void OnOpen() {
    // Optional implementation by derived class
  }

  /**
   * @brief Callback to notify when dialog is closed
   */
  virtual void OnClose() {
    // Optional implementation by derived class
  }

  /* ******************************************************************************************** */
  //! Variables

  bool opened_ = false;  //!< Flag to indicate dialog visilibity
  Size size_;            //!< Dialog size settings
  Style style_;          //!< Color style
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_DIALOG_H_
