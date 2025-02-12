#include "view/element/button.h"

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
 * @class GraphicButton
 * @brief Base class interface for all media buttons that use Canvas for custom drawing
 */
class GraphicButton : public Button {
 public:
  explicit GraphicButton(const Style& style, const Callback& on_click)
      : Button(style, on_click, /*active*/ true) {}

  //! Override base class method to implement custom rendering
  ftxui::Element RenderImpl() override {
    ftxui::Canvas content = Draw();

    auto button = ftxui::canvas(content) | ftxui::hcenter | ftxui::border | ftxui::reflect(box_);

    const auto& border_color = !focused_ ? style_.normal.border : style_.focused.border;

    return button | ftxui::color(border_color);
  }

  /**
   * @brief Custom drawing (implemented by derived class)
   */
  virtual ftxui::Canvas Draw() const = 0;

 protected:
  //! To make life easier
  using Point = std::pair<int, int>;

  static constexpr int kWidth = 12;   //!< Width size for Canvas
  static constexpr int kHeight = 12;  //!< Height size for Canvas
};

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_play(const Callback& on_click) {
  class Play : public GraphicButton {
   public:
    using GraphicButton::GraphicButton;

    //! Override base class method to implement custom drawing
    ftxui::Canvas Draw() const override { return !clicked_ ? DrawPlay() : DrawPause(); }

    //! Draw Play button
    ftxui::Canvas DrawPlay() const {
      ftxui::Canvas play(kWidth, kHeight);

      auto [a_x, a_y] = Point{3, 0};
      auto [b_x, b_y] = Point{9, 6};
      auto [c_x, c_y] = Point{3, 11};

      const auto& color = style_.normal.foreground;

      for (int i = 1; i < 6; ++i) {
        play.DrawPointLine(a_x + i, a_y + i, b_x - i, b_y - i, color);
        play.DrawPointLine(b_x - i, b_y - i, c_x + i, c_y - i, color);
        play.DrawPointLine(c_x + i, c_y - i, a_x + i, a_y + i, color);
      }

      return play;
    }

    // Draw Pause button
    ftxui::Canvas DrawPause() const {
      // pause
      ftxui::Canvas pause(kWidth, kHeight);

      auto [g_x, g_y] = Point{2, 1};
      auto [h_x, h_y] = Point{2, 10};
      int space = 6;

      const auto& color = style_.normal.foreground;

      for (int i = 0; i < 2; ++i) {
        pause.DrawPointLine(g_x + i, g_y, h_x + i, h_y, color);
        pause.DrawPointLine(g_x + i + space, g_y, h_x + i + space, h_y, color);
      }

      return pause;
    }
  };

  auto style = Style{
      .normal =
          Style::State{.foreground = ftxui::Color::SpringGreen2, .border = ftxui::Color::GrayDark},

      .focused = Style::State{.border = ftxui::Color::SteelBlue3},
  };

  return std::make_shared<Play>(style, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_stop(const Callback& on_click) {
  class Stop : public GraphicButton {
    using GraphicButton::GraphicButton;

    //! Override base class method to implement custom drawing
    ftxui::Canvas Draw() const override {
      // stop
      ftxui::Canvas stop(kWidth, kHeight);

      for (int i = 1; i < 11; ++i) stop.DrawPointLine(2, i, 9, i, style_.normal.foreground);

      return stop;
    }
  };

  auto style = Style{
      .normal =
          Style::State{
              .foreground = ftxui::Color::Red,
              .border = ftxui::Color::GrayDark,
          },

      .focused = Style::State{.border = ftxui::Color::SteelBlue3},
  };

  return std::make_shared<Stop>(style, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_skip_previous(const Callback& on_click) {
  class SkipPrevious : public GraphicButton {
   public:
    using GraphicButton::GraphicButton;

    //! Override base class method to implement custom drawing
    ftxui::Canvas Draw() const override {
      ftxui::Canvas skip_next(kWidth, kHeight);

      auto [a_x, a_y] = Point{8, 1};
      auto [b_x, b_y] = Point{3, 5};
      auto [c_x, c_y] = Point{8, 10};

      const auto& color = style_.normal.foreground;

      for (int i = 0; i < 6; ++i) {
        skip_next.DrawPointLine(a_x - i, a_y + i, b_x + i, b_y, color);
        skip_next.DrawPointLine(b_x + i, b_y, c_x, c_y, color);
        skip_next.DrawPointLine(c_x, c_y, a_x - i, a_y + i, color);
      }

      auto [d_x, d_y] = Point{3, 1};
      auto [e_x, e_y] = Point{3, 10};

      skip_next.DrawPointLine(d_x, d_y, e_x, e_y, color);
      skip_next.DrawPointLine(d_x - 1, d_y, e_x - 1, e_y, color);

      return skip_next;
    }
  };

  auto style = Style{
      .normal =
          Style::State{.foreground = ftxui::Color::SteelBlue, .border = ftxui::Color::GrayDark},

      .focused = Style::State{.border = ftxui::Color::SteelBlue3},
  };

  return std::make_shared<SkipPrevious>(style, on_click);
}
/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_skip_next(const Callback& on_click) {
  class SkipNext : public GraphicButton {
   public:
    using GraphicButton::GraphicButton;

    //! Override base class method to implement custom drawing
    ftxui::Canvas Draw() const override {
      ftxui::Canvas skip_next(kWidth, kHeight);

      auto [a_x, a_y] = Point{2, 0};
      auto [b_x, b_y] = Point{8, 6};
      auto [c_x, c_y] = Point{2, 11};

      const auto& color = style_.normal.foreground;

      for (int i = 1; i < 6; ++i) {
        skip_next.DrawPointLine(a_x + i, a_y + i, b_x - i, b_y - i, color);
        skip_next.DrawPointLine(b_x - i, b_y - i, c_x + i, c_y - i, color);
        skip_next.DrawPointLine(c_x + i, c_y - i, a_x + i, a_y + i, color);
      }

      auto [d_x, d_y] = Point{8, 1};
      auto [e_x, e_y] = Point{8, 10};

      skip_next.DrawPointLine(d_x, d_y, e_x, e_y, color);
      skip_next.DrawPointLine(d_x + 1, d_y, e_x + 1, e_y, color);

      return skip_next;
    }
  };

  auto style = Style{
      .normal =
          Style::State{.foreground = ftxui::Color::SteelBlue, .border = ftxui::Color::GrayDark},

      .focused = Style::State{.border = ftxui::Color::SteelBlue3},
  };

  return std::make_shared<SkipNext>(style, on_click);
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

      ftxui::Decorator style;
      bool invert = focused_;

      if (focused_) {
        style = Apply(selected_ ? style_.selected : style_.focused, invert);
      } else if (parent_focused_) {
        style = Apply(selected_ ? style_.selected : style_.normal, invert);
      } else {
        style = selected_ ? ApplyReverse(style_.normal) : Apply(style_.normal);
      }

      return ftxui::hbox({left, content, right}) | style | ftxui::reflect(box_);
    }

    std::string content_;
  };

  return std::make_shared<WindowButton>(style, content, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button(const ftxui::Element& content, const Callback& on_click,
                                            const Style& style, bool active) {
  class GenericButton : public Button {
   public:
    explicit GenericButton(const Style& style, const ftxui::Element& content,
                           const Callback& on_click, bool active)
        : Button(style, on_click, active), content_{content} {}

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

      return hbox({left, content_, right}) | style | border | ftxui::reflect(box_);
    }

    ftxui::Element content_;
  };

  return std::make_shared<GenericButton>(style, content, on_click, active);
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
