#include "view/element/button.h"

namespace interface {

Button::Button(ftxui::Color style_content, ftxui::Color style_border, Callback on_click)
    : box_(),
      focused_(false),
      style_(ButtonStyles{
          .content = style_content,
          .border = style_border,
      }),
      on_click_(on_click) {}

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
      return true;
    }

  } else {
    focused_ = false;
  }

  return false;
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_play(ftxui::Color style_content,
                                                 ftxui::Color style_border, Callback on_click) {
  class Play : public Button {
   public:
    explicit Play(ftxui::Color style_content, ftxui::Color style_border, Callback on_click)
        : Button(style_content, style_border, on_click) {}

    //! Override base class method to implement custom rendering
    ftxui::Element Render() override {
      // play
      ftxui::Canvas play(12, 12);

      using Point = std::pair<int, int>;
      auto a = Point{3, 0};
      auto b = Point{9, 6};
      auto c = Point{3, 11};

      for (int i = 1; i < 6; ++i) {
        play.DrawPointLine(a.first + i, a.second + i, b.first - i, b.second - i, style_.content);
        play.DrawPointLine(b.first - i, b.second - i, c.first + i, c.second - i, style_.content);
        play.DrawPointLine(c.first + i, c.second - i, a.first + i, a.second + i, style_.content);
      }

      auto button_play =
          ftxui::canvas(std::move(play)) | ftxui::hcenter | ftxui::border | ftxui::reflect(box_);

      if (!focused_)
        button_play = button_play | ftxui::color(style_.border);
      else  // TODO: change aquamarine color
        button_play = button_play | ftxui::color(ftxui::Color::Aquamarine1Bis);

      return button_play;
    }

    // TODO: pause state
    // void pause() {
    //   // pause
    //   ftxui::Canvas pause(12, 12);
    //   auto g = Point{2, 1};
    //   auto h = Point{2, 10};
    //   int space = 6;

    //   for (int i = 0; i < 2; ++i) {
    //     pause.DrawPointLine(g.first + i, g.second, h.first + i, h.second, play_color);
    //     pause.DrawPointLine(g.first + i + space, g.second, h.first + i + space, h.second,
    //     play_color);
    //   }
    //   auto button_pause = ftxui::canvas(std::move(pause)) | ftxui::hcenter | ftxui::bold |
    //                       ftxui::border | ftxui::color(border_color);
    // }
  };

  return std::make_shared<Play>(style_content, style_border, on_click);
}

/* ********************************************************************************************** */

std::shared_ptr<Button> Button::make_button_stop(ftxui::Color style_content,
                                                 ftxui::Color style_border, Callback on_click) {
  class Stop : public Button {
   public:
    explicit Stop(ftxui::Color style_content, ftxui::Color style_border, Callback on_click)
        : Button(style_content, style_border, on_click) {}

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
        button_stop = button_stop | ftxui::color(style_.border);
      else  // TODO: change aquamarine
        button_stop = button_stop | ftxui::color(ftxui::Color::Aquamarine1Bis);

      return button_stop;
    }
  };

  return std::make_shared<Stop>(style_content, style_border, on_click);
}

/* ********************************************************************************************** */

// OLD ELEMENTS (TODO: REMOVE IT WHEN NO LONGER NEEDED)
//   // play
//   ftxui::Canvas play(20, 20);
//   for (int i = 1; i < 10; ++i) {
//     play.DrawPointLine(5 + i, 0 + i, 15 - i, 10 - i, play_color);
//     play.DrawPointLine(15 - i, 10 - i, 5 + i, 19 - i, play_color);
//     play.DrawPointLine(5 + i, 19 - i, 5 + i, 0 + i, play_color);
//   }

//   // pause
//   ftxui::Canvas pause(20, 20);
//   for (int i = 0; i < 3; ++i) {
//     pause.DrawPointLine(5 + i, 0, 5 + i, 19, play_color);
//     pause.DrawPointLine(12 + i, 0, 12 + i, 19, play_color);
//   }
//   auto button_pause = ftxui::canvas(std::move(pause)) | ftxui::hcenter | ftxui::bold |
//                       ftxui::border | ftxui::color(border_color);

//   // stop
//   ftxui::Canvas stop(20, 20);
//   for (int i = 0; i < 20; ++i) {
//     stop.DrawPointLine(3, i, 16, i, stop_color);
//   }
//   auto button_stop = ftxui::canvas(std::move(stop)) | ftxui::hcenter | ftxui::bold |
//   ftxui::border | ftxui::color(border_color);

}  // namespace interface