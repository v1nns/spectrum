#include "view/block/audio_visualizer.h"

#include <chrono>
#include <cmath>
#include <iterator>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"

namespace interface {

/* ********************************************************************************************** */

AudioVisualizer::AudioVisualizer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::AudioVisualizer, interface::Size{.width = 0, .height = 0}},
      active_view_{TabView::Visualizer},
      btn_help_{nullptr},
      btn_exit_{nullptr},
      curr_anim_{Animation::HorizontalMirror},
      spectrum_data_{},
      filter_bars_{model::AudioFilter::Create()} {
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
}

/* ********************************************************************************************** */

ftxui::Element AudioVisualizer::Render() {
  auto get_decorator_for = [&](const TabView& view) {
    return (active_view_ == view) ? ftxui::nothing : ftxui::color(ftxui::Color::GrayDark);
  };

  ftxui::Element title_border = ftxui::hbox({
      ftxui::text(" "),
      ftxui::text("1:visualizer") | get_decorator_for(TabView::Visualizer),
      ftxui::text(" "),
      ftxui::text("2:equalizer") | get_decorator_for(TabView::Equalizer),
      ftxui::text(" "),
      ftxui::filler(),
      btn_help_->Render(),
      ftxui::text(" ") | ftxui::border,  // dummy space between buttons
      btn_exit_->Render(),
  });

  ftxui::Element view = ftxui::text("");

  switch (active_view_) {
    case TabView::Visualizer:
      view = DrawVisualizer();
      break;

    case TabView::Equalizer:
      view = DrawEqualizer();
      break;

    case TabView::LAST:
      ERROR("Audio visualizer active view contains invalid value");
      active_view_ = TabView::Visualizer;
      break;
  }

  return ftxui::window(title_border, view | ftxui::yflex);
}

/* ********************************************************************************************** */

bool AudioVisualizer::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  // Change active view to visualizer
  if (event == ftxui::Event::Character('1') && active_view_ != TabView::Visualizer) {
    active_view_ = TabView::Visualizer;
    return true;
  }

  // Change active view to equalizer
  if (event == ftxui::Event::Character('2') && active_view_ != TabView::Equalizer) {
    active_view_ = TabView::Equalizer;
    return true;
  }

  // Send new animation to terminal
  if (event == ftxui::Event::Character('a')) {
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    spectrum_data_.clear();

    curr_anim_ = static_cast<Animation>((curr_anim_ + 1) % Animation::LAST);
    auto event = CustomEvent::ChangeBarAnimation(curr_anim_);
    dispatcher->SendEvent(event);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool AudioVisualizer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::DrawAudioSpectrum) {
    spectrum_data_ = event.GetContent<std::vector<double>>();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool AudioVisualizer::OnMouseEvent(ftxui::Event event) {
  if (btn_help_->OnEvent(event)) return true;

  if (btn_exit_->OnEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

ftxui::Element AudioVisualizer::DrawVisualizer() {
  ftxui::Element bar_visualizer = ftxui::text("");

  switch (curr_anim_) {
    case Animation::HorizontalMirror:
      DrawAnimationHorizontalMirror(bar_visualizer);
      break;

    case Animation::VerticalMirror:
      DrawAnimationVerticalMirror(bar_visualizer);
      break;

    case Animation::LAST:
      ERROR("Audio visualizer current animation contains invalid value");
      curr_anim_ = HorizontalMirror;
      break;
  }

  return bar_visualizer;
}

/* ********************************************************************************************** */

void AudioVisualizer::DrawAnimationHorizontalMirror(ftxui::Element& visualizer) {
  int size = spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * 4;
  entries.reserve(total_size);

  for (int i = (size / 2) - 1; i >= 0; i--) {
    entries.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::text(" "));
  }

  for (int i = size / 2; i < size; i++) {
    entries.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::text(" "));
  }

  visualizer = ftxui::hbox(std::move(entries)) | ftxui::hcenter;
}

/* ********************************************************************************************** */

void AudioVisualizer::DrawAnimationVerticalMirror(ftxui::Element& visualizer) {
  int size = spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements left, right;

  // Preallocate memory
  int total_size = (size / 2) * 4;
  left.reserve(total_size);
  right.reserve(total_size);

  for (int i = 0; i < size / 2; i++) {
    left.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    left.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    left.push_back(ftxui::gaugeUp(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    left.push_back(ftxui::text(" "));
  }

  for (int i = size / 2; i < size; i++) {
    right.push_back(ftxui::gaugeDown(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    right.push_back(ftxui::gaugeDown(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    right.push_back(ftxui::gaugeDown(spectrum_data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    right.push_back(ftxui::text(" "));
  }

  visualizer = ftxui::vbox(ftxui::hbox(left) | ftxui::hcenter | ftxui::yflex,
                           ftxui::hbox(right) | ftxui::hcenter | ftxui::yflex);
}

/* ********************************************************************************************** */

ftxui::Element AudioVisualizer::DrawEqualizer() {
  // TODO: implement
  ftxui::Elements frequencies;

  frequencies.push_back(ftxui::filler());

  auto gen_slider = [](double value) {
    return ftxui::gaugeUp(value) | ftxui::yflex_grow |
           ftxui::bgcolor(ftxui::Color::LightSteelBlue3) | ftxui::color(ftxui::Color::SteelBlue3);
  };

  // Iterate through all audio filters
  for (const auto& filter : filter_bars_) {
    float gain = filter.GetGainAsPercentage();

    frequencies.push_back(ftxui::vbox({
        ftxui::text(""),
        ftxui::text(filter.GetFrequency()) | ftxui::hcenter,
        ftxui::text(""),
        ftxui::hbox({gen_slider(gain), gen_slider(gain)}) | ftxui::hcenter | ftxui::yflex_grow,
        ftxui::text(""),
        ftxui::text(filter.GetGain()) | ftxui::inverted | ftxui::hcenter |
            ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 7),
        ftxui::text(""),
    }));

    frequencies.push_back(ftxui::filler());
  }

  return ftxui::hbox(frequencies);
}

}  // namespace interface
