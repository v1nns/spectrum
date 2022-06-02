#include "view/element/button.h"

namespace interface {

Button::Button(ButtonStyles style, Callback on_click)
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
      // Mouse click on menu entry
      //   on_click_();
      clicked_ = !clicked_;
      return true;
    }

  } else {
    focused_ = false;
  }

  return false;
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_play(Callback on_click) {
  class Play : public Button {
   public:
    explicit Play(ButtonStyles style, Callback on_click) : Button(style, on_click) {}

    //! Override base class method to implement custom rendering
    ftxui::Element Render() override {
      ftxui::Canvas content = !clicked_ ? DrawPlay() : DrawPause();

      auto button =
          ftxui::canvas(std::move(content)) | ftxui::hcenter | ftxui::border | ftxui::reflect(box_);

      if (!focused_)
        button = button | ftxui::color(style_.border_normal);
      else
        button = button | ftxui::color(style_.border_focused);

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
      .border_normal = ftxui::Color::Black,
      .border_focused = ftxui::Color::Aquamarine1Bis,
  };

  return std::make_shared<Play>(style, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_stop(Callback on_click) {
  class Stop : public Button {
   public:
    explicit Stop(ButtonStyles style, Callback on_click) : Button(style, on_click) {}

    //! Override base class method to implement custom rendering
    ftxui::Element Render() override {
      // stop
      ftxui::Canvas stop(12, 12);

      for (int i = 1; i < 11; ++i) {
        stop.DrawPointLine(2, i, 9, i, style_.content);
      }

      auto button_stop =
          ftxui::canvas(std::move(stop)) | ftxui::hcenter | ftxui::border | ftxui::reflect(box_);

      if (!focused_)
        button_stop = button_stop | ftxui::color(style_.border_normal);
      else
        button_stop = button_stop | ftxui::color(style_.border_focused);

      return button_stop;
    }
  };

  auto style = ButtonStyles{
      .content = ftxui::Color::Red,
      .border_normal = ftxui::Color::Black,
      .border_focused = ftxui::Color::Aquamarine1Bis,
  };

  return std::make_shared<Stop>(style, on_click);
}

}  // namespace interface