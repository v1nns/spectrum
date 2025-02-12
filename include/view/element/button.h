/**
 * \file
 * \brief  Class for rendering a customized button
 */

#ifndef INCLUDE_VIEW_ELEMENT_BUTTON_H_
#define INCLUDE_VIEW_ELEMENT_BUTTON_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

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

  /**
   * @brief Style for each part of the button, most of them are optional to use it (check button
   * implementation for more info)
   */
  struct Style {
    struct State {
      ftxui::Color foreground;  //!< Color for button foreground
      ftxui::Color background;  //!< Color for button background
      ftxui::Color border;      //!< Color for border
    };

    State normal;    //!< Colors for normal state
    State focused;   //!< Colors for focused state
    State selected;  //!< Colors for selected state
    State pressed;   //!< Colors for pressed state
    State disabled;  //!< Colors for disabled state

    int height;  //!< Fixed height for button
    int width;   //!< Fixed width for button

    std::optional<Delimiters> delimiters;  //!< Used by window buttons as a custom border
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
  explicit Button(const Style& style, Callback on_click, bool active);

  /**
   * @brief Create a style with given button state colors (foreground and background)
   * @param colors Button state colors
   * @param invert Flag to invert foreground with background color
   * @return ftxui::Decorator Style decorator
   */
  inline ftxui::Decorator Apply(const Style::State& colors, bool invert = false) {
    return ftxui::bgcolor(colors.background) | ftxui::color(colors.foreground) |
           (invert ? ftxui::inverted : ftxui::nothing);
  }

  /**
   * @brief Create a style and switch colors from the given button state colors
   * @param colors Button state colors
   * @return ftxui::Decorator Style decorator
   */
  inline ftxui::Decorator ApplyReverse(const Style::State& colors) {
    return ftxui::bgcolor(colors.foreground) | ftxui::color(colors.background);
  }

  /**
   * @brief Determine colors based on current button state
   * @return ftxui::Decorator Style decorator
   */
  inline const Style::State& GetStateColors() const {
    if (!enabled_) {
      return style_.disabled;
    }

    if (focused_) {
      if (pressed_)
        return style_.pressed;
      else
        return style_.focused;
    }

    if (selected_) {
      return style_.selected;
    }

    return style_.normal;
  }

  /**
   * @brief Renders the component (implemented by derived)
   * @return Element Built element based on internal state
   */
  virtual ftxui::Element RenderImpl() = 0;

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
  static std::shared_ptr<Button> make_button_play(const Callback& on_click);

  /**
   * @brief Create a Stop button
   * @param on_click Callback function for click event
   * @return std::shared_ptr<Button> New instance to Stop button
   */
  static std::shared_ptr<Button> make_button_stop(const Callback& on_click);

  /**
   * @brief Create a Skip to Previous Song button
   * @param on_click Callback function for click event
   * @return std::shared_ptr<Button> New instance to Skip to Previous Song button
   */
  static std::shared_ptr<Button> make_button_skip_previous(const Callback& on_click);

  /**
   * @brief Create a Skip to Next Song button
   * @param on_click Callback function for click event
   * @return std::shared_ptr<Button> New instance to Skip to Next Song button
   */
  static std::shared_ptr<Button> make_button_skip_next(const Callback& on_click);

  /**
   * @brief Create button for the border of the block window
   * @param content Text content to show
   * @param on_click Callback function for click event
   * @param style Custom style to apply on button
   * @return std::shared_ptr<Button> New instance to Window button
   */
  static std::shared_ptr<Button> make_button_for_window(const std::string& content,
                                                        const Callback& on_click,
                                                        const Style& style);

  /**
   * @brief Create generic button
   * @param content Custom ftxui::Element to display
   * @param on_click Callback function for click event
   * @param style Custom style to apply on button
   * @param active Button state (if it is clickable or not)
   * @return std::shared_ptr<Button> New instance to button
   */
  static std::shared_ptr<Button> make_button(const ftxui::Element& content,
                                             const Callback& on_click, const Style& style,
                                             bool active = true);

  /**
   * @brief Create generic button with solid color
   * @param content Text content to show
   * @param on_click Callback function for click event
   * @param style Custom style to apply on button
   * @param active Button state (if it is clickable or not)
   * @return std::shared_ptr<Button> New instance to button
   */
  static std::shared_ptr<Button> make_button_solid(const std::string& content,
                                                   const Callback& on_click, const Style& style,
                                                   bool active = true);

  /* ******************************************************************************************** */
  //! Public API for Button

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
  bool OnMouseEvent(ftxui::Event event);

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
   * @brief Set button state to enabled
   */
  void Enable();

  /**
   * @brief Set button state to disabled
   */
  void Disable();

  /**
   * @brief Set button state to selected
   */
  void Select();

  /**
   * @brief Set button state to unselected
   */
  void Unselect();

  /**
   * @brief Set owner focus state (OPTIONAL, used mainly by window buttons)
   * @param focused Flag indicating if parent (UI element that created button) is focused or not
   */
  void UpdateParentFocus(bool focused);

  /**
   * @brief Get button state
   */
  bool IsActive() const;

  /**
   * @brief Execute button callback function
   */
  void OnClick() const;

  /* ******************************************************************************************** */
  //! Internal handling
 private:
  //! Handle left click event internally
  bool HandleLeftClick(ftxui::Event& event);

  /* ******************************************************************************************** */
  //! Variables
 protected:
  ftxui::Box box_;         //!< Box to control if mouse cursor is over the button
  bool enabled_ = false;   //!< Flag to indicate if button is enabled (can be clicked)
  bool focused_ = false;   //!< Flag to indicate if button is focused (mouse on hover)
  bool selected_ = false;  //!< Flag to indicate if button is selected (set by component owner)
  bool clicked_ = false;   //!< Flag to indicate if button was clicked (mouse click)
  bool pressed_ = false;   //!< Flag to indicate if button is pressed (mouse hold click)
  bool parent_focused_ = false;  //!< Flag to indicate if owner is focused

  Style style_;  //!< Color style for each part of the button

  Callback on_click_;  //!< Callback function to trigger when button is clicked
};

//! For readability
using MediaButton = std::shared_ptr<Button>;
using WindowButton = std::shared_ptr<Button>;
using GenericButton = std::shared_ptr<Button>;

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_BUTTON_H_
