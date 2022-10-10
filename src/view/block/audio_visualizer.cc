#include "view/block/audio_visualizer.h"

#include <chrono>
#include <cmath>
#include <iterator>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace interface {

/* ********************************************************************************************** */

AudioVisualizer::AudioVisualizer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::AudioVisualizer, interface::Size{.width = 0, .height = 0}},
      data_{} {}

/* ********************************************************************************************** */

ftxui::Element AudioVisualizer::Render() {
  ftxui::Elements entries;

  int size = data_.size();
  if (size > 0) {
    // for (int i = 0; i < size / 2; i++) {
    for (int i = (size / 2) - 1; i >= 0; i--) {
      entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
      entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
      entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
      entries.push_back(ftxui::text(" "));
    }

    // for (int i = size - 1; i >= size / 2; i--) {
    for (int i = size / 2; i <= size; i++) {
      entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
      entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
      entries.push_back(ftxui::gaugeUp(data_[i]) | ftxui::color(ftxui::Color::SteelBlue3));
      entries.push_back(ftxui::text(" "));
    }
  }

  return ftxui::window(ftxui::text(" visualizer "),
                       ftxui::hbox(std::move(entries)) | ftxui::hcenter | ftxui::yflex);
}

/* ********************************************************************************************** */

bool AudioVisualizer::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

bool AudioVisualizer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::DrawAudioSpectrum) {
    data_ = event.GetContent<std::vector<double>>();
    return true;
  }

  return false;
}

}  // namespace interface
