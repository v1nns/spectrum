#include "view/block/audio_visualizer.h"

#include <chrono>
#include <cmath>
#include <iterator>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace interface {

/* ********************************************************************************************** */

AudioVisualizer::AudioVisualizer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::AudioVisualizer}, data_{} {}

/* ********************************************************************************************** */

ftxui::Element AudioVisualizer::Render() {
  // ftxui::Element content = ftxui::vbox(ftxui::filler());
  ftxui::Elements entries;

  int size = data_.size();
  if (size > 0) {
    entries.push_back(ftxui::text(" "));

    for (int i = 0; i < size / 2; i++) {
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::text(" "));
    }

    for (int i = size - 1; i >= size / 2; i--) {
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::gaugeUp(data_[i]));
      entries.push_back(ftxui::text(" "));
    }
  }

  return ftxui::window(ftxui::text(" visualizer "), ftxui::hbox(std::move(entries)) | ftxui::yflex);
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
