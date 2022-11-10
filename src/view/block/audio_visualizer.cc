#include "view/block/audio_visualizer.h"

#include <chrono>
#include <cmath>
#include <iterator>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"

namespace interface {

/* ********************************************************************************************** */

AudioVisualizer::AudioVisualizer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::AudioVisualizer, interface::Size{.width = 0, .height = 0}},
      data_{},
      curr_anim_{Animation::HorizontalMirror} {}

/* ********************************************************************************************** */

ftxui::Element AudioVisualizer::Render() {
  ftxui::Element bar_visualizer = ftxui::text("");

  switch (curr_anim_) {
    case HorizontalMirror:
      DrawAnimationHorizontalMirror(bar_visualizer);
      break;

    case VerticalMirror:
      DrawAnimationVerticalMirror(bar_visualizer);
      break;

    case LAST:
      ERROR("Audio visualizer current animation contais value equal to LAST");
      curr_anim_ = HorizontalMirror;
      break;
  }

  return ftxui::window(ftxui::text(" visualizer "), bar_visualizer | ftxui::yflex);
}  // namespace interface

/* ********************************************************************************************** */

bool AudioVisualizer::OnEvent(ftxui::Event event) {
  // Send new animation to terminal
  if (event == ftxui::Event::Character('a')) {
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    data_.clear();

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
    data_ = event.GetContent<std::vector<double>>();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void AudioVisualizer::DrawAnimationHorizontalMirror(ftxui::Element& visualizer) {
  int size = data_.size();
  if (size == 0) return;

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * 4;
  entries.reserve(total_size);

  for (int i = (size / 2) - 1; i >= 0; i--) {
    entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::text(" "));
  }

  for (int i = size / 2; i < size; i++) {
    entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::text(" "));
  }

  visualizer = ftxui::hbox(std::move(entries)) | ftxui::hcenter;
}

/* ********************************************************************************************** */

void AudioVisualizer::DrawAnimationVerticalMirror(ftxui::Element& visualizer) {
  int size = data_.size();
  if (size == 0) return;

  ftxui::Elements left, right;

  // Preallocate memory
  int total_size = (size / 2) * 4;
  left.reserve(total_size);
  right.reserve(total_size);

  for (int i = 0; i < size / 2; i++) {
    left.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    left.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    left.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    left.push_back(ftxui::text(" "));
  }

  for (int i = size / 2; i < size; i++) {
    right.push_back(ftxui::gaugeDown(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    right.push_back(ftxui::gaugeDown(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    right.push_back(ftxui::gaugeDown(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    right.push_back(ftxui::text(" "));
  }

  visualizer = ftxui::vbox(ftxui::hbox(left) | ftxui::hcenter | ftxui::yflex,
                           ftxui::hbox(right) | ftxui::hcenter | ftxui::yflex);
}

}  // namespace interface
