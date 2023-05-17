#include "view/block/tab_item/spectrum_visualizer.h"

#include <algorithm>

#include "util/logger.h"

namespace interface {

SpectrumVisualizer::SpectrumVisualizer(const model::BlockIdentifier& id,
                                       const std::shared_ptr<EventDispatcher>& dispatcher,
                                       const FocusCallback& on_focus)
    : TabItem(id, dispatcher, on_focus) {}

/* ********************************************************************************************** */

ftxui::Element SpectrumVisualizer::Render() {
  ftxui::Element bar_visualizer = ftxui::text("");

  switch (curr_anim_) {
    case model::BarAnimation::HorizontalMirror:
      DrawAnimationHorizontalMirror(bar_visualizer);
      break;

    case model::BarAnimation::VerticalMirror:
      DrawAnimationVerticalMirror(bar_visualizer);
      break;

    case model::BarAnimation::Mono:
      DrawAnimationMono(bar_visualizer);
      break;

    case model::BarAnimation::LAST:
      ERROR("Audio visualizer current animation contains invalid value");
      curr_anim_ = model::BarAnimation::HorizontalMirror;
      break;
  }

  return bar_visualizer;
}

/* ********************************************************************************************** */

bool SpectrumVisualizer::OnEvent(const ftxui::Event& event) {
  // Notify terminal to recalculate new size for spectrum data
  if (event == ftxui::Event::Character('a')) {
    LOG("Handle key to change audio animation");
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    spectrum_data_.clear();
    curr_anim_ = curr_anim_ < model::BarAnimation::Mono
                     ? static_cast<model::BarAnimation>(curr_anim_ + 1)  // get next
                     : model::BarAnimation::HorizontalMirror;            // reset to first one

    auto event_animation = CustomEvent::ChangeBarAnimation(curr_anim_);
    dispatcher->SendEvent(event_animation);

    return true;
  }

  // Enable/disable fullscreen mode with spectrum visualizer
  if (event == ftxui::Event::Character('h')) {
    LOG("Handle key to toggle visualizer in fullscreen mode");
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    auto event_toggle = CustomEvent::ToggleFullscreen();
    dispatcher->SendEvent(event_toggle);
  }

  return false;
}

/* ********************************************************************************************** */

bool SpectrumVisualizer::OnCustomEvent(const CustomEvent& event) {
  // Store spectrum audio data to render later
  if (event == CustomEvent::Identifier::DrawAudioSpectrum) {
    spectrum_data_ = event.GetContent<std::vector<double>>();
    return true;
  }

  // Calculate new number of bars based on current animation
  if (event == CustomEvent::Identifier::CalculateNumberOfBars) {
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    int number_bars = event.GetContent<int>();

    // To fill entire screen, multiply value by 2
    if (curr_anim_ == model::BarAnimation::VerticalMirror ||
        curr_anim_ == model::BarAnimation::Mono) {
      number_bars *= 2;
    }

    auto event_resize = CustomEvent::ResizeAnalysis(number_bars);
    dispatcher->SendEvent(event_resize);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::CreateGauge(float value, ftxui::Direction direction,
                                     ftxui::Elements& elements) const {
  constexpr auto color = [](const ftxui::Direction& dir) {
    auto gradient = ftxui::LinearGradient()
                        .Angle(dir == ftxui::Direction::Up ? 270 : 90)
                        .Stop(ftxui::Color(95, 135, 215), 0.0f)
                        .Stop(ftxui::Color(115, 155, 215), 0.3f)
                        .Stop(ftxui::Color(155, 188, 235), 0.6f)
                        .Stop(ftxui::Color(185, 208, 252), 0.8f);

    return ftxui::color(gradient);
  };

  for (int i = 0; i < kGaugeThickness - 1; i++) {
    elements.push_back(ftxui::gaugeDirection(value, direction) | color(direction));
  }

  elements.push_back(ftxui::text(" "));
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationHorizontalMirror(ftxui::Element& visualizer) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * kGaugeThickness;
  entries.reserve(total_size);

  for (int i = (size / 2) - 1; i >= 0; i--) {
    CreateGauge((float)spectrum_data_[i], ftxui::Direction::Up, entries);
  }

  for (int i = size / 2; i < size; i++) {
    CreateGauge((float)spectrum_data_[i], ftxui::Direction::Up, entries);
  }

  visualizer = ftxui::hbox(entries) | ftxui::hcenter;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationVerticalMirror(ftxui::Element& visualizer) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements left;
  ftxui::Elements right;

  // Preallocate memory
  int total_size = (size / 2) * kGaugeThickness;
  left.reserve(total_size);
  right.reserve(total_size);

  for (int i = 0; i < size / 2; i++) {
    CreateGauge((float)spectrum_data_[i], ftxui::Direction::Up, left);
  }

  for (int i = size / 2; i < size; i++) {
    CreateGauge((float)spectrum_data_[i], ftxui::Direction::Down, right);
  }

  visualizer = ftxui::vbox(ftxui::hbox(left) | ftxui::hcenter | ftxui::yflex,
                           ftxui::hbox(right) | ftxui::hcenter | ftxui::yflex);
}

/* ********************************************************************************************** */

void SpectrumVisualizer::DrawAnimationMono(ftxui::Element& visualizer) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  // As total size is equal to the sum of both channels, this animation is the average of both
  // channels, so divide size by 2
  size /= 2;

  // Split data by channel
  std::vector<double>::const_iterator first = spectrum_data_.begin();
  std::vector<double>::const_iterator middle = spectrum_data_.begin() + size;
  std::vector<double>::const_iterator last = spectrum_data_.end();

  std::vector<double> left(first, middle);
  std::vector<double> right(middle, last);
  std::vector<double> average(size, 0);

  // Get average of each frequency from channels
  std::transform(left.begin(), left.end(),  // Left channel
                 right.begin(),             // Right channel
                 average.begin(),           // Average of the sum from both
                 [](double a, double b) { return (a + b) / 2; });

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * kGaugeThickness;
  entries.reserve(total_size);

  for (int i = 0; i < size; i++) {
    CreateGauge((float)spectrum_data_[i], ftxui::Direction::Up, entries);
  }

  visualizer = ftxui::hbox(entries) | ftxui::hcenter;
}

}  // namespace interface
