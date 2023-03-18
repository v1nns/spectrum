#include "view/block/tab_viewer.h"

#include "util/logger.h"
#include "view/block/tab_item/audio_equalizer.h"
#include "view/block/tab_item/spectrum_visualizer.h"

namespace interface {

/* ********************************************************************************************** */

TabViewer::TabViewer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, model::BlockIdentifier::TabViewer,
            interface::Size{.width = 0, .height = 0}},
      btn_help_{nullptr},
      btn_exit_{nullptr},
      active_{View::Visualizer},
      views_{} {
  // Initialize window buttons
  btn_help_ = Button::make_button_for_window(std::string("F1:help"), [&]() {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) return false;

    LOG("Handle left click mouse event on Help button");
    auto event = interface::CustomEvent::ShowHelper();
    dispatcher->SendEvent(event);

    return true;
  });

  btn_exit_ = Button::make_button_for_window(std::string("X"), [&]() {
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    LOG("Handle left click mouse event on Exit button");
    auto event = interface::CustomEvent::Exit();
    dispatcher->SendEvent(event);

    return true;
  });

  // Add tab views
  views_[View::Visualizer] = Tab{
      .key = std::string{"1"},
      .button = Button::make_button_for_window(
          std::string{"1:visualizer"},
          [&]() {
            LOG("Handle left click mouse event on Tab button for visualizer");
            active_ = View::Visualizer;

            // Send event to set focus on this block
            AskForFocus();

            return true;
          },
          Button::Delimiters{" ", " "}),
      .item = std::make_unique<SpectrumVisualizer>(GetId(), dispatcher),
  };

  views_[View::Equalizer] = Tab{
      .key = std::string{"2"},
      .button = Button::make_button_for_window(
          std::string{"2:equalizer"},
          [&]() {
            LOG("Handle left click mouse event on Tab button for equalizer");
            active_ = View::Equalizer;

            // Send event to set focus on this block
            AskForFocus();

            return true;
          },
          Button::Delimiters{" ", " "}),
      .item = std::make_unique<AudioEqualizer>(GetId(), dispatcher),
  };
}

/* ********************************************************************************************** */

ftxui::Element TabViewer::Render() {
  auto get_decorator_for = [&](const View& v) {
    return (active_ == v) ? GetTitleDecorator() : ftxui::color(ftxui::Color::GrayDark);
  };

  auto btn_visualizer = views_[View::Visualizer].button->Render();
  auto btn_equalizer = views_[View::Equalizer].button->Render();

  ftxui::Element title_border = ftxui::hbox({
      btn_visualizer | get_decorator_for(View::Visualizer),
      btn_equalizer | get_decorator_for(View::Equalizer),
      ftxui::filler(),
      btn_help_->Render(),
      ftxui::text(" ") | ftxui::border,  // dummy space between buttons
      btn_exit_->Render(),
  });

  ftxui::Element view = active()->Render();

  return ftxui::window(title_border, view | ftxui::yflex);
}

/* ********************************************************************************************** */

bool TabViewer::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  // Check if event is equal to a registered keybinding for any of the tab items
  auto found = std::find_if(views_.begin(), views_.end(),
                            [event](auto&& t) { return t.second.key == event.character(); });

  // Found some mapped keybinding, now check if this is already the active view
  if (found != views_.end()) {
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      // Set this block as active (focused)
      auto event = interface::CustomEvent::SetFocused(GetId());
      dispatcher->SendEvent(event);
    }

    // Update active tab
    if (active_ != found->first) active_ = found->first;

    return true;
  }

  // If block is not focused, do not even try to handle event
  if (!IsFocused()) {
    return false;
  }

  // Otherwise, let item handle it
  return active()->OnEvent(event);
}

/* ********************************************************************************************** */

bool TabViewer::OnCustomEvent(const CustomEvent& event) { return active()->OnCustomEvent(event); }

/* ********************************************************************************************** */

bool TabViewer::OnMouseEvent(ftxui::Event event) {
  if (btn_help_->OnEvent(event)) return true;

  if (btn_exit_->OnEvent(event)) return true;

  for (auto& view : views_) {
    if (view.second.button->OnEvent(event)) {
      return true;
    }
  }

  return active()->OnMouseEvent(event);
}

}  // namespace interface
