#include "view/block/audio_visualizer.h"

#include <chrono>
#include <cmath>
#include <iterator>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace interface {

/* ********************************************************************************************** */

AudioVisualizer::AudioVisualizer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::AudioVisualizer}, data_() {}

/* ********************************************************************************************** */

ftxui::Element AudioVisualizer::Render() {
  // ftxui::Element content = ftxui::vbox(ftxui::filler());
  ftxui::Elements entries;

  for (const auto& value : data_) {
    entries.push_back(ftxui::gauge(value));
    entries.push_back(ftxui::text(" "));
  }

  return ftxui::window(ftxui::text(" visualizer "), ftxui::vbox(std::move(entries)) | ftxui::flex);
}

/* ********************************************************************************************** */

bool AudioVisualizer::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

bool AudioVisualizer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::DrawAudioSpectrum) {
    const auto& data = event.GetContent<std::vector<double>>();

    // TODO: do something with data
    data_ = std::move(data);

    return true;
  }

  return false;
}

}  // namespace interface
