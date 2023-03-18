#include "view/block/tab_item/spectrum_visualizer.h"

#include <algorithm>

#include "util/logger.h"

namespace interface {

SpectrumVisualizer::SpectrumVisualizer(const model::BlockIdentifier& id,
                                       const std::shared_ptr<EventDispatcher>& dispatcher)
    : TabItem(id, dispatcher),
      curr_anim_{model::BarAnimation::HorizontalMirror},
      spectrum_data_{} {}

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

bool SpectrumVisualizer::OnEvent(ftxui::Event event) {
  // Notify terminal to recalculate new size for spectrum data
  if (event == ftxui::Event::Character('a')) {
    LOG("Handle key to change audio animation");
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    spectrum_data_.clear();
    curr_anim_ = curr_anim_ < model::BarAnimation::Mono
                     ? static_cast<model::BarAnimation>(curr_anim_ + 1)  // get next
                     : model::BarAnimation::HorizontalMirror;            // reset to first one

    auto event = CustomEvent::ChangeBarAnimation(curr_anim_);
    dispatcher->SendEvent(event);

    return true;
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

void SpectrumVisualizer::DrawAnimationVerticalMirror(ftxui::Element& visualizer) {
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

void SpectrumVisualizer::DrawAnimationMono(ftxui::Element& visualizer) {
  int size = spectrum_data_.size();
  if (size == 0) return;

  // As total size is equal to the sum of both channels, this animation is the average of both
  // channels, so divide size by 2
  size /= 2;

  // Split data by channel
  std::vector<double>::const_iterator first = spectrum_data_.begin();
  std::vector<double>::const_iterator middle = spectrum_data_.begin() + size;
  std::vector<double>::const_iterator last = spectrum_data_.end();

  std::vector<double> left(first, middle), right(middle, last), average(size, 0);

  // Get average of each frequency from channels
  std::transform(left.begin(), left.end(),  // Left channel
                 right.begin(),             // Right channel
                 average.begin(),           // Average of the sum from both
                 [](double a, double b) { return (a + b) / 2; });

  ftxui::Elements entries;

  // Preallocate memory
  int total_size = size * 4;
  entries.reserve(total_size);

  for (int i = 0; i < size; i++) {
    entries.push_back(ftxui::gaugeUp(average[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(average[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::gaugeUp(average[i]) | ftxui::color(ftxui::Color::SteelBlue3));
    entries.push_back(ftxui::text(" "));
  }

  visualizer = ftxui::hbox(std::move(entries)) | ftxui::hcenter;
}

}  // namespace interface