#include "view/block/tab_item/spectrum_visualizer.h"

#include <algorithm>

#include "util/logger.h"

namespace interface {

SpectrumVisualizer::SpectrumVisualizer(const model::BlockIdentifier& id,
                                       const std::shared_ptr<EventDispatcher>& dispatcher)
    : TabItem(id, dispatcher) {}

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

  return false;
}

/* ********************************************************************************************** */

void SpectrumVisualizer::CreateGauge(float value, Direction direction,
                                     ftxui::Elements& elements) const {
  ftxui::GaugeDirection dir =
      direction == Direction::Up ? ftxui::GaugeDirection::Up : ftxui::GaugeDirection::Down;

  for (int i = 0; i < kGaugeThickness - 1; i++) {
    elements.push_back(ftxui::gaugeDirection(value, dir) | ftxui::color(ftxui::Color::SteelBlue3));
  }

  elements.push_back(ftxui::text(" "));
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

void SpectrumVisualizer::DrawAnimationHorizontalMirror(ftxui::Element& visualizer) {
  auto size = (int)spectrum_data_.size();
  if (size == 0) return;

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * kGaugeThickness;
  entries.reserve(total_size);

  for (int i = (size / 2) - 1; i >= 0; i--) {
    CreateGauge((float)spectrum_data_[i], Direction::Up, entries);
  }

  for (int i = size / 2; i < size; i++) {
    CreateGauge((float)spectrum_data_[i], Direction::Up, entries);
  }

  visualizer = ftxui::hbox(std::move(entries)) | ftxui::hcenter;
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
    CreateGauge((float)spectrum_data_[i], Direction::Up, left);
  }

  for (int i = size / 2; i < size; i++) {
    CreateGauge((float)spectrum_data_[i], Direction::Down, right);
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
    CreateGauge((float)spectrum_data_[i], Direction::Up, entries);
  }

  visualizer = ftxui::hbox(std::move(entries)) | ftxui::hcenter;
}

}  // namespace interface