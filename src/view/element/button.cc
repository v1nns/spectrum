#include "view/element/button.h"

namespace interface {

Button::Button(const ButtonStyles& style, Callback on_click)
    : box_{}, focused_{false}, clicked_{false}, style_{style}, on_click_{on_click} {}

/* ********************************************************************************************** */

bool Button::OnEvent(ftxui::Event event) {
  if (event.mouse().button == ftxui::Mouse::WheelDown ||
      event.mouse().button == ftxui::Mouse::WheelUp) {
    return false;
  }

  if (event.mouse().button != ftxui::Mouse::None && event.mouse().button != ftxui::Mouse::Left) {
    return false;
  }

  //   if (!CaptureMouse(event)) return false;

  if (box_.Contain(event.mouse().x, event.mouse().y)) {
    focused_ = true;

    if (event.mouse().button == ftxui::Mouse::Left &&
        event.mouse().motion == ftxui::Mouse::Released) {
      ToggleState();

      // Mouse click on menu entry
      if (on_click_ != nullptr) on_click_();

      return true;
    }

  } else {
    focused_ = false;
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

std::shared_ptr<Button> Button::make_button_play(Callback on_click) {
  class Play : public Button {
   public:
    explicit Play(const ButtonStyles& style, Callback on_click) : Button(style, on_click) {}

    //! Override base class method to implement custom rendering
    ftxui::Element Render() override {
      ftxui::Canvas content = !clicked_ ? DrawPlay() : DrawPause();

      auto button =
          ftxui::canvas(std::move(content)) | ftxui::hcenter | ftxui::border | ftxui::reflect(box_);

      ftxui::Decorator border_color =
          !focused_ ? ftxui::color(style_.border_normal) : ftxui::color(style_.border_focused);

      button = button | border_color;
      return button;
    }

   private:
    //! To make life easier
    using Point = std::pair<int, int>;

    //! Draw Play button
    ftxui::Canvas DrawPlay() {
      // play
      ftxui::Canvas play(12, 12);

      auto a = Point{3, 0};
      auto b = Point{9, 6};
      auto c = Point{3, 11};

      for (int i = 1; i < 6; ++i) {
        play.DrawPointLine(a.first + i, a.second + i, b.first - i, b.second - i, style_.content);
        play.DrawPointLine(b.first - i, b.second - i, c.first + i, c.second - i, style_.content);
        play.DrawPointLine(c.first + i, c.second - i, a.first + i, a.second + i, style_.content);
      }

      return play;
    }

    // Draw Pause button
    ftxui::Canvas DrawPause() {
      // pause
      ftxui::Canvas pause(12, 12);
      auto g = Point{2, 1};
      auto h = Point{2, 10};
      int space = 6;

      for (int i = 0; i < 2; ++i) {
        pause.DrawPointLine(g.first + i, g.second, h.first + i, h.second, style_.content);
        pause.DrawPointLine(g.first + i + space, g.second, h.first + i + space, h.second,
                            style_.content);
      }

      return pause;
    }
  };

  auto style = ButtonStyles{
      .content = ftxui::Color::SpringGreen2,
      .border_normal = ftxui::Color::GrayDark,
      .border_focused = ftxui::Color::SteelBlue3,
  };

  return std::make_shared<Play>(style, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_stop(Callback on_click) {
  class Stop : public Button {
   public:
    explicit Stop(const ButtonStyles& style, Callback on_click) : Button(style, on_click) {}

    //! Override base class method to implement custom rendering
    ftxui::Element Render() override {
      // stop
      ftxui::Canvas stop(12, 12);

      for (int i = 1; i < 11; ++i) {
        stop.DrawPointLine(2, i, 9, i, style_.content);
      }

      auto button_stop =
          ftxui::canvas(std::move(stop)) | ftxui::hcenter | ftxui::border | ftxui::reflect(box_);

      ftxui::Decorator border_color =
          !focused_ ? ftxui::color(style_.border_normal) : ftxui::color(style_.border_focused);

      button_stop = button_stop | border_color;
      return button_stop;
    }
  };

  auto style = ButtonStyles{
      .content = ftxui::Color::Red,
      .border_normal = ftxui::Color::GrayDark,
      .border_focused = ftxui::Color::SteelBlue3,
  };

  return std::make_shared<Stop>(style, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_for_window(const std::string& content,
                                                       Callback on_click) {
  class WindowButton : public Button {
   public:
    explicit WindowButton(const ButtonStyles& style, const std::string& content, Callback on_click)
        : Button(style, on_click), content_{content} {}

    //! Override base class method to implement custom rendering
    ftxui::Element Render() override {
      auto left = ftxui::text("[") | ftxui::bold;
      auto right = ftxui::text("]") | ftxui::bold;
      auto content = ftxui::text(content_);

      ftxui::Decorator decorator = ftxui::nothing;
      if (focused_) decorator = ftxui::color(style_.content) | ftxui::inverted;

      return ftxui::hbox({left, std::move(content), right}) | decorator | ftxui::reflect(box_);
    }

    std::string content_;
  };

  auto style = ButtonStyles{
      .content = ftxui::Color::White,
      // not using the rest...
  };

  return std::make_shared<WindowButton>(style, content, on_click);
}

}  // namespace interface
