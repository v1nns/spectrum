#include "view/element/button.h"

#include "view/element/style.h"

namespace interface {

Button::Button(const Style& style, Callback on_click, bool active)
    : enabled_{active}, style_{style}, on_click_{on_click} {}

/* ********************************************************************************************** */

ftxui::Element Button::Render() {
  using ftxui::EQUAL;
  using ftxui::HEIGHT;
  using ftxui::WIDTH;

  ftxui::Decorator style = ftxui::nothing;

  // Apply size constraints
  if (style_.height) style = style | ftxui::size(HEIGHT, EQUAL, style_.height);
  if (style_.width) style = style | ftxui::size(WIDTH, EQUAL, style_.width);

  return RenderImpl() | style;
}

/* ********************************************************************************************** */

bool Button::OnMouseEvent(ftxui::Event event) {
  if (event.mouse().button == ftxui::Mouse::WheelDown ||
      event.mouse().button == ftxui::Mouse::WheelUp) {
    return false;
  }

  if (box_.Contain(event.mouse().x, event.mouse().y)) {
    focused_ = true;

    if (enabled_ && event.mouse().button == ftxui::Mouse::Left) {
      return HandleLeftClick(event);
    }
  } else {
    // Clear some states
    focused_ = false;
    pressed_ = false;
  }

  return false;
}

/* ********************************************************************************************** */

void Button::SetState(bool clicked) { clicked_ = clicked; }

/* ********************************************************************************************** */

void Button::ToggleState() { clicked_ = !clicked_; }

/* ********************************************************************************************** */

void Button::ResetState() { clicked_ = false; }

/* ********************************************************************************************** */

void Button::Enable() {
  if (!enabled_) enabled_ = true;
}

/* ********************************************************************************************** */

void Button::Disable() {
  if (enabled_) enabled_ = false;
}

/* ********************************************************************************************** */

void Button::Select() { selected_ = true; }

/* ********************************************************************************************** */

void Button::Unselect() { selected_ = false; }

/* ********************************************************************************************** */

void Button::UpdateParentFocus(bool focused) { parent_focused_ = focused; }

/* ********************************************************************************************** */

bool Button::IsActive() const { return enabled_; }

/* ********************************************************************************************** */

void Button::OnClick() const {
  if (on_click_) on_click_();
}

/* ********************************************************************************************** */

bool Button::HandleLeftClick(ftxui::Event& event) {
  // Mouse click hold
  if (event.mouse().motion == ftxui::Mouse::Pressed) {
    pressed_ = true;
  }

  // Mouse click released
  if (event.mouse().motion == ftxui::Mouse::Released) {
    // Update internal state
    pressed_ = false;

    // Trigger callback for button click and change clicked state
    if (on_click_) {
      // Only change internal state if owner's callback did something
      clicked_ = on_click_();
    }

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

/**
 * @class IconButton
 * @brief Media button shown as an icon in a single line, with another icon for when it is clicked
 */
class IconButton : public Button {
 public:
  //! Get color for icon from theme
  using IconColor = std::function<ftxui::Color()>;

  explicit IconButton(const Callback& on_click, const std::string& icon,
                      const std::string& icon_clicked, const IconColor& color)
      : Button(Style{}, on_click, /*active*/ true),
        icon_{icon},
        icon_clicked_{icon_clicked},
        color_{color} {}

  //! Override base class method to implement custom rendering
  ftxui::Element RenderImpl() override {
    // Space around icon is also part of button, to make it easier to click
    const std::string content = " " + (clicked_ ? icon_clicked_ : icon_) + " ";

    // Icon keeps its color, mouse over button changes only what is behind it
    ftxui::Decorator style = ftxui::color(color_());
    if (focused_) style = style | ftxui::bgcolor(GetTheme().player.button_hovered);

    return ftxui::text(content) | style | ftxui::reflect(box_);
  }

 private:
  std::string icon_;          //!< Icon shown by default
  std::string icon_clicked_;  //!< Icon shown after button is clicked (e.g. pause instead of play)
  IconColor color_;           //!< Color for icon
};

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_play(const Callback& on_click) {
  return std::make_shared<IconButton>(on_click, "▶ ", "∥ ", [] { return GetTheme().player.play; });
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_stop(const Callback& on_click) {
  return std::make_shared<IconButton>(on_click, "■ ", "■ ", [] { return GetTheme().player.stop; });
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_skip_previous(const Callback& on_click) {
  return std::make_shared<IconButton>(on_click, "◀◀", "◀◀", [] { return GetTheme().player.skip; });
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_skip_next(const Callback& on_click) {
  return std::make_shared<IconButton>(on_click, "▶▶", "▶▶", [] { return GetTheme().player.skip; });
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_for_window(const std::string& content,
                                                       const Callback& on_click,
                                                       const Style& style) {
  class WindowButton : public Button {
   public:
    explicit WindowButton(const Style& style, const std::string& content, const Callback& on_click)
        : Button(style, on_click, true), content_{content} {}

    //! Override base class method to implement custom rendering
    ftxui::Element RenderImpl() override {
      ftxui::Element left = ftxui::text(std::get<0>(*style_.delimiters)) | ftxui::bold;
      ftxui::Element right = ftxui::text(std::get<1>(*style_.delimiters)) | ftxui::bold;
      ftxui::Element content = ftxui::text(content_);

      content |= (parent_focused_ || focused_) ? ftxui::bold : ftxui::nothing;

      const Style::Colors colors = GetColors();

      ftxui::Decorator style;
      bool invert = focused_;

      if (focused_) {
        style = Apply(selected_ ? colors.selected : colors.focused, invert);
      } else if (parent_focused_) {
        style = Apply(selected_ ? colors.selected : colors.normal, invert);
      } else {
        style = selected_ ? ApplyReverse(colors.normal) : Apply(colors.normal);
      }

      return ftxui::hbox({left, content, right}) | style | ftxui::reflect(box_);
    }

    std::string content_;
  };

  return std::make_shared<WindowButton>(style, content, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button(const std::string& content, const Callback& on_click,
                                            const Style& style, const std::string& letter,
                                            bool active) {
  class GenericButton : public Button {
   public:
    explicit GenericButton(const Style& style, const std::string& content,
                           const std::string& letter, const Callback& on_click, bool active)
        : Button(style, on_click, active), content_{content} {
      // Find letter in text and save index
      if (!letter.empty()) {
        if (size_t index = content.find(letter); index != std::string::npos) {
          letter_ = letter;
          index_to_highlight_ = index;
        }
      }
    }

    //! Override base class method to implement custom rendering
    ftxui::Element RenderImpl() override {
      using ftxui::Decorator, ftxui::Element, ftxui::emptyElement, ftxui::hbox, ftxui::text;

      const Style::State& colors = GetStateColors();
      const bool custom_border = style_.delimiters.has_value();

      Element left, right;
      Decorator style;
      Decorator border;

      if (custom_border) {
        left = text(std::get<0>(*style_.delimiters));
        right = text(std::get<1>(*style_.delimiters));
        style = Apply(colors, pressed_);
        border = ftxui::nothing;

      } else {
        left = emptyElement();
        right = emptyElement();
        style = ftxui::center | Apply(colors, pressed_);
        border = ftxui::borderLight | ftxui::color(colors.border);
      }

      return hbox({left, GetElement(), right}) | style | border | ftxui::reflect(box_);
    }

    //! Custom logic to highlight a single letter used as keybind
    ftxui::Element GetElement() const {
      if (!index_to_highlight_.has_value()) {
        return ftxui::text(content_);
      }

      ftxui::Element before(ftxui::text(content_.substr(0, *index_to_highlight_)));
      ftxui::Element letter(ftxui::text(letter_.has_value() ? *letter_ : ""));
      ftxui::Element after(ftxui::text(content_.substr(*index_to_highlight_ + 1)));

      ftxui::Decorator color =
          enabled_ && !pressed_ ? ftxui::color(GetColors().highlight.foreground) : ftxui::nothing;

      return ftxui::hbox({before, letter | ftxui::bold | ftxui::underlined | color, after});
    }

    std::string content_;
    std::optional<std::string> letter_;
    std::optional<size_t> index_to_highlight_;
  };

  return std::make_shared<GenericButton>(style, content, letter, on_click, active);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_hint(const std::string& key, const std::string& content,
                                                 const Callback& on_click, const Style& style) {
  class HintButton : public Button {
   public:
    explicit HintButton(const Style& style, const std::string& key, const std::string& content,
                        const Callback& on_click)
        : Button(style, on_click, true), key_{key}, content_{" " + content} {}

    //! Override base class method to implement custom rendering
    ftxui::Element RenderImpl() override {
      const Style::State& colors = GetStateColors();

      // Key only stands out while it does something
      ftxui::Decorator key = enabled_ ? Foreground(GetColors().highlight.foreground) | ftxui::bold
                                      : Foreground(colors.foreground);

      return ftxui::hbox({
                 ftxui::text(key_) | key,
                 ftxui::text(content_) | Foreground(colors.foreground),
             }) |
             ftxui::reflect(box_);
    }

    std::string key_;
    std::string content_;
  };

  return std::make_shared<HintButton>(style, key, content, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_solid(const std::string& content,
                                                  const Callback& on_click, const Style& style,
                                                  bool active) {
  class SolidButton : public Button {
   public:
    explicit SolidButton(const Style& style, const std::string& content, const Callback& on_click,
                         bool active)
        : Button(style, on_click, active), content_{content} {}

    //! Override base class method to implement custom rendering
    ftxui::Element RenderImpl() override {
      const Style::State& colors = GetStateColors();

      ftxui::Element content = ftxui::text(content_);
      ftxui::Decorator style = ftxui::borderLight | Apply(colors, pressed_);

      return ftxui::hbox(content) | ftxui::center | style | ftxui::reflect(box_);
    }

    std::string content_;
  };

  return std::make_shared<SolidButton>(style, content, on_click, active);
}

}  // namespace interface
