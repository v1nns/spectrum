#include "view/block/tab_viewer.h"

#include "util/logger.h"
#include "view/block/tab_item/audio_equalizer.h"
#include "view/block/tab_item/spectrum_visualizer.h"

namespace interface {

/* ********************************************************************************************** */

TabViewer::TabViewer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::TabViewer, interface::Size{.width = 0, .height = 0}},
      btn_help_{nullptr},
      btn_exit_{nullptr},
      active_{View::Visualizer},
      views_{} {
  btn_help_ = Button::make_button_for_window(std::string("F1:help"), [&]() {
    LOG("Handle left click mouse event on Help button");
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      auto event = interface::CustomEvent::ShowHelper();
      dispatcher->SendEvent(event);
    }
  });

  btn_exit_ = Button::make_button_for_window(std::string("X"), [&]() {
    LOG("Handle left click mouse event on Exit button");
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      auto event = interface::CustomEvent::Exit();
      dispatcher->SendEvent(event);
    }
  });

  // Add tab views
  views_[View::Visualizer] = std::make_unique<SpectrumVisualizer>(dispatcher);
  views_[View::Equalizer] = std::make_unique<AudioEqualizer>(dispatcher);
}

/* ********************************************************************************************** */

ftxui::Element TabViewer::Render() {
  auto get_decorator_for = [&](const View& v) {
    return (active_ == v) ? ftxui::nothing : ftxui::color(ftxui::Color::GrayDark);
  };

  auto visualizer = views_[View::Visualizer]->GetTitle();
  auto equalizer = views_[View::Equalizer]->GetTitle();

  ftxui::Element title_border = ftxui::hbox({
      ftxui::text(" "),
      ftxui::text(visualizer) | get_decorator_for(View::Visualizer),
      ftxui::text(" "),
      ftxui::text(equalizer) | get_decorator_for(View::Equalizer),
      ftxui::text(" "),
      ftxui::filler(),
      btn_help_->Render(),
      ftxui::text(" ") | ftxui::border,  // dummy space between buttons
      btn_exit_->Render(),
  });

  ftxui::Element view = views_[active_]->Render();

  return ftxui::window(title_border, view | ftxui::yflex);
}

/* ********************************************************************************************** */

bool TabViewer::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  // Change active view to visualizer
  if (event == ftxui::Event::Character('1') && active_ != View::Visualizer) {
    active_ = View::Visualizer;
    return true;
  }

  // Change active view to equalizer
  if (event == ftxui::Event::Character('2') && active_ != View::Equalizer) {
    active_ = View::Equalizer;
    return true;
  }

  return views_[active_]->OnEvent(event);
}

/* ********************************************************************************************** */

bool TabViewer::OnCustomEvent(const CustomEvent& event) {
  return views_[active_]->OnCustomEvent(event);
}

/* ********************************************************************************************** */

bool TabViewer::OnMouseEvent(ftxui::Event event) {
  if (btn_help_->OnEvent(event)) return true;

  if (btn_exit_->OnEvent(event)) return true;

  return views_[active_]->OnMouseEvent(event);
}

}  // namespace interface
