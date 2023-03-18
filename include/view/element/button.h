/**
 * \file
 * \brief  Class for rendering a customized button
 */

#ifndef INCLUDE_VIEW_ELEMENT_BUTTON_H_
#define INCLUDE_VIEW_ELEMENT_BUTTON_H_

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "ftxui/component/event.hpp"  // for Event
#include "ftxui/dom/elements.hpp"     // for Element

namespace interface {

/**
 * @brief Interface for customized button using Canvas internally to draw content
 */
class Button {
 public:
  //! Just to make life easier
  using Callback = std::function<bool()>;  //!< Return true to toggle state, otherwise false
  using Delimiter = std::string;           //!< Character delimiter for window buttons
  using Delimiters = std::tuple<Delimiter, Delimiter>;  //!< Left and right

 private:
  //! Style for each part of the button
  struct ButtonStyle {
    ftxui::Color content;
    ftxui::Color border_normal;
    ftxui::Color border_focused;

    int height, width;
    Delimiters delimiters;
  };

 protected:
  /**
   * @brief Construct a new Button object, but this cannot be done directly, must use one of the
   * public methods for creation (based on factory pattern)
   *
   * @param styles Color style for button
   * @param on_click Callback function for click event
   * @param active Button state (if it is clickable or not)
   */
  explicit Button(const ButtonStyle& style, Callback on_click, bool active);

 public:
  /**
   * @brief Destroy Button object
   */
  virtual ~Button() = default;

  /* ******************************************************************************************** */
  //! Button creation based on factory pattern

  /**
   * @brief Create a Play button
   * @param on_click Callback function for click event
   * @return std::shared_ptr<Button> New instance to Play button
   */
  static std::shared_ptr<Button> make_button_play(Callback on_click);

  /**
   * @brief Create a Stop button
   * @param on_click Callback function for click event
   * @return std::shared_ptr<Button> New instance to Stop button
   */
  static std::shared_ptr<Button> make_button_stop(Callback on_click);

  /**
   * @brief Create button for the border of the block window
   * @param content Text content to show
   * @param on_click Callback function for click event
   * @param delimiter Character delimiter to use as border
   * @return std::shared_ptr<Button> New instance to Window button
   */
  static std::shared_ptr<Button> make_button_for_window(
      const std::string& content, Callback on_click,
      const Delimiters& delimiters = std::make_tuple("[", "]"));

  /**
   * @brief Create generic button
   * @param content Text content to show
   * @param on_click Callback function for click event
   * @param active Button state (if it is clickable or not)
   * @return std::shared_ptr<Button> New instance to button
   */
  static std::shared_ptr<Button> make_button(const std::string& content, Callback on_click,
                                             bool active = true);

  /* ******************************************************************************************** */
  //! Public API for Button

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  virtual ftxui::Element Render() = 0;

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event);

  /**
   * @brief Set button state
   */
  void SetState(bool clicked);

  /**
   * @brief Toggle button state
   */
  void ToggleState();

  /**
   * @brief Reset button state to initial value
   */
  void ResetState();

  /**
   * @brief Set button state to active
   */
  void SetActive();

  /**
   * @brief Set button state to inactive
   */
  void SetInactive();

  /**
   * @brief Get button state
   */
  bool IsActive() const;

  /**
   * @brief Execute button callback function
   */
  void OnClick();

  /* ******************************************************************************************** */
 protected:
  ftxui::Box box_;  //!< Box to control if mouse cursor is over the button
  bool active_;     //!< Flag to indicate if button is activated (can be clicked)
  bool focused_;    //!< Flag to indicate if button is focused (mouse on hover)
  bool clicked_;    //!< Flag to indicate if button was clicked (mouse click)
  bool pressed_;    //!< Flag to indicate if button is pressed (mouse hold click)

  ButtonStyle style_;  //!< Color style for each part of the button

  Callback on_click_;  //!< Callback function to trigger when button is clicked
};

//! For readability
using MediaButton = std::shared_ptr<Button>;
using WindowButton = std::shared_ptr<Button>;
using GenericButton = std::shared_ptr<Button>;

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_BUTTON_H_
