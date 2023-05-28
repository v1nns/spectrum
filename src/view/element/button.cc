#include "view/element/button.h"

namespace interface {

Button::Button(const ButtonStyle& style, Callback on_click, bool active)
    : enabled_{active}, style_{style}, on_click_{on_click} {}

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
      on_click_();
      clicked_ = !clicked_;
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
  explicit GraphicButton(const ButtonStyle& style, const Callback& on_click)
      : Button(style, on_click, /*active*/ true) {}

  //! Override base class method to implement custom rendering
  ftxui::Element Render() override {
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

  auto style = ButtonStyle{
      .normal = ButtonStyle::State{.foreground = ftxui::Color::SpringGreen2,
                                   .border = ftxui::Color::GrayDark},

      .focused = ButtonStyle::State{.border = ftxui::Color::SteelBlue3},
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

  auto style = ButtonStyle{
      .normal =
          ButtonStyle::State{
              .foreground = ftxui::Color::Red,
              .border = ftxui::Color::GrayDark,
          },

      .focused = ButtonStyle::State{.border = ftxui::Color::SteelBlue3},
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

  auto style = ButtonStyle{
      .normal = ButtonStyle::State{.foreground = ftxui::Color::SteelBlue,
                                   .border = ftxui::Color::GrayDark},

      .focused = ButtonStyle::State{.border = ftxui::Color::SteelBlue3},
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

  auto style = ButtonStyle{
      .normal = ButtonStyle::State{.foreground = ftxui::Color::SteelBlue,
                                   .border = ftxui::Color::GrayDark},

      .focused = ButtonStyle::State{.border = ftxui::Color::SteelBlue3},
  };

  return std::make_shared<SkipNext>(style, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_for_window(const std::string& content,
                                                       const Callback& on_click,
                                                       const ButtonStyle& style) {
  class WindowButton : public Button {
   public:
    explicit WindowButton(const ButtonStyle& style, const std::string& content,
                          const Callback& on_click)
        : Button(style, on_click, true), content_{content} {}

    //! Override base class method to implement custom rendering
    ftxui::Element Render() override {
      auto left = ftxui::text(std::get<0>(style_.delimiters)) | ftxui::bold;
      auto right = ftxui::text(std::get<1>(style_.delimiters)) | ftxui::bold;
      auto content = ftxui::text(content_);

      constexpr auto create_style = [](const ftxui::Color& bg, const ftxui::Color& fg,
                                       bool invert) {
        return ftxui::bgcolor(bg) | ftxui::color(fg) | (invert ? ftxui::inverted : ftxui::nothing);
      };

      // Based on internal state, determine which style to apply
      ftxui::Color const* background;
      ftxui::Color const* foreground;
      bool invert = false;

      if (focused_) {
        background = selected_ ? &style_.selected.background : &style_.focused.background;
        foreground = selected_ ? &style_.selected.foreground : &style_.focused.foreground;
        invert = !selected_;

      } else if (parent_focused_) {
        background = selected_ ? &style_.selected.background : &style_.normal.background;
        foreground = selected_ ? &style_.selected.foreground : &style_.normal.foreground;

      } else {
        // As we don't to highlight selected colors, we just invert the normal
        background = selected_ ? &style_.normal.foreground : &style_.normal.background;
        foreground = selected_ ? &style_.normal.background : &style_.normal.foreground;
      }

      auto style = create_style(*background, *foreground, invert);

      return ftxui::hbox({left, content, right}) | style | ftxui::reflect(box_);
    }

    std::string content_;
  };

  return std::make_shared<WindowButton>(style, content, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button(const std::string& content, const Callback& on_click,
                                            bool active) {
  class GenericButton : public Button {
   public:
    explicit GenericButton(const ButtonStyle& style, const std::string& content,
                           const Callback& on_click, bool active)
        : Button(style, on_click, active), content_{content} {}

    //! Override base class method to implement custom rendering
    ftxui::Element Render() override {
      using ftxui::EQUAL;
      using ftxui::HEIGHT;
      using ftxui::WIDTH;

      auto content = ftxui::text(content_);

      // default decorator
      ftxui::Decorator style = ftxui::center;

      style = style | ftxui::borderLight;
      if (style_.height) style = style | ftxui::size(HEIGHT, EQUAL, style_.height);
      if (style_.width) style = style | ftxui::size(WIDTH, EQUAL, style_.width);

      if (enabled_) {
        const auto& content_color = style_.normal.foreground;
        style = style | ftxui::color(content_color) | (focused_ ? ftxui::inverted : ftxui::nothing);
      } else {
        style = style | ftxui::color(ftxui::Color::GrayDark);
      }

      // TODO: think about this
      //   if (pressed_) decorator = decorator | ftxui::bgcolor(ftxui::Color::Aquamarine1);

      return ftxui::hbox(content) | style | ftxui::reflect(box_);
    }

    std::string content_;
  };

  auto style = ButtonStyle{
      .normal =
          ButtonStyle::State{.foreground = ftxui::Color::White, .border = ftxui::Color::GrayDark},

      .focused = ButtonStyle::State{.border = ftxui::Color::SteelBlue3},
      .width = 15,
  };

  return std::make_shared<GenericButton>(style, content, on_click, active);
}

}  // namespace interface
