/**
 * \file
 * \brief  Class for rendering a customized button
 */

#ifndef INCLUDE_VIEW_ELEMENT_BUTTON_H_
#define INCLUDE_VIEW_ELEMENT_BUTTON_H_

#include <memory>
#include <utility>

#include "ftxui/component/event.hpp"  // for Event
#include "ftxui/dom/elements.hpp"     // for Element

namespace interface {

/**
 * @brief Interface for customized button using Canvas internally to draw content
 */
class Button {
  //! Just to make life easier
  using Callback = std::function<void()>;

 protected:
  /**
   * @brief Construct a new Button object, but this cannot be done directly, must use one of the
   * public methods for creation (based on factory pattern)
   *
   * @param style_content Color for content
   * @param style_border Color for border
   * @param on_click Callback function for click event
   */
  explicit Button(ftxui::Color style_content, ftxui::Color style_border, Callback on_click);

 public:
  /**
   * @brief Create a Play button
   * @return std::shared_ptr<Button> New instance to Play button
   */
  static std::shared_ptr<Button> make_button_play(ftxui::Color style_content,
                                                  ftxui::Color style_border, Callback on_click);

  /**
   * @brief Create a Stop button
   * @return std::shared_ptr<Button> New instance to Stop button
   */
  static std::shared_ptr<Button> make_button_stop(ftxui::Color style_content,
                                                  ftxui::Color style_border, Callback on_click);

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

  /* ******************************************************************************************** */
 private:
  //! Style for each part of the button
  struct ButtonStyles {
    ftxui::Color content;
    ftxui::Color border;
  };

  /* ******************************************************************************************** */
 protected:
  ftxui::Box box_;  //!< Box to control if mouse cursor is over the button
  bool focused_;    //!< Flag to indicate if button is focused

  ButtonStyles style_;  //!< Color style for each part of the button

  Callback on_click_;  //!< Callback function to trigger when button is clicked
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_BUTTON_H_