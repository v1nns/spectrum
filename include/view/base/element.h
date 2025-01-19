/**
 * \file
 * \brief  Base class for any element displayed in the UI
 */

#ifndef INCLUDE_VIEW_BASE_ELEMENT_H_
#define INCLUDE_VIEW_BASE_ELEMENT_H_

#include <chrono>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

namespace interface {

//! Base class for elements
class Element {
 public:
  //! Default destructor
  virtual ~Element() = default;

  /* ******************************************************************************************** */
  //! Optional Implementation

  /**
   * @brief Renders the element
   * @return Element Built element based on internal state
   */
  virtual ftxui::Element Render();

  /**
   * @brief Handles an event from keyboard
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  virtual bool OnEvent(const ftxui::Event& event);

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  virtual bool OnMouseEvent(ftxui::Event& event);

  /**
   * @brief Handles an action key from keyboard
   * @param event Received event from screen
   */
  virtual bool HandleActionKey(const ftxui::Event& event);

  /**
   * @brief Handles a click event from mouse
   * @param event Received event from screen
   */
  virtual void HandleClick(ftxui::Event& event);

  /**
   * @brief Handles a double click event from mouse
   * @param event Received event from screen
   */
  virtual void HandleDoubleClick(ftxui::Event& event);

  /**
   * @brief Handles a hover event from mouse
   * @param event Received event from screen
   */
  virtual void HandleHover(ftxui::Event& event);

  /**
   * @brief Handles a mouse wheel event
   * @param button Received button from mouse wheel event
   */
  virtual void HandleWheel(const ftxui::Mouse::Button& button);

  /* ******************************************************************************************** */
  //! Getters and setters

  /**
   * @brief Getter for hover state
   * @return true if mouse cursor is hovered on element, otherwise false
   */
  bool IsHovered() const { return hovered_; }

  /**
   * @brief Getter for focus state
   * @return true if element is focused, otherwise false
   */
  bool IsFocused() const { return focused_; }

  /**
   * @brief Getter for box element
   * @return Box reference
   */
  ftxui::Box& Box() { return box_; }

  /**
   * @brief Getter for box element
   * @return Box copy
   */
  const ftxui::Box& Box() const { return box_; }

  /**
   * @brief Set hover state
   * @param enable Flag to enable/disable state
   */
  void SetHover(bool enable) { hovered_ = enable; }

  /**
   * @brief Set focus state
   * @param enable Flag to enable/disable state
   */
  void SetFocus(bool enable) {
    focused_ = enable;
    OnFocusChanged();
  }

  /* ******************************************************************************************** */
  //! Optional implementation

  /**
   * @brief Notify when focus state has changed
   */
  virtual void OnFocusChanged() { /* optional */ }

  /* ******************************************************************************************** */
  //! Variables
 private:
  ftxui::Box box_;        //!< Box to control if mouse cursor is over the element
  bool hovered_ = false;  //!< Flag to indicate if element is hovered (by mouse)
  bool focused_ = false;  //!< Flag to indicate if element is focused (set by equalizer)

  std::chrono::system_clock::time_point last_click_;  //!< Last timestamp that mouse was clicked
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_ELEMENT_H_
