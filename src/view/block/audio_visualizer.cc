#include "view/block/audio_visualizer.h"

#include <fstream>
#include <iostream>
#include <iterator>

#include "ftxui/component/event.hpp"

namespace interface {

/* ********************************************************************************************** */

AudioVisualizer::AudioVisualizer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Block{dispatcher, Identifier::AudioVisualizer} {}

/* ********************************************************************************************** */

ftxui::Element AudioVisualizer::Render() {
  ftxui::Element content = ftxui::vbox(ftxui::filler());
  return ftxui::window(ftxui::text(" visualizer "), std::move(content));
}

/* ********************************************************************************************** */

bool AudioVisualizer::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

bool AudioVisualizer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Type::DrawAudioRaw) {
    const auto& data = event.GetContent<std::vector<int>>();

    // TODO: do something with data

    return true;
  }

  return false;
}

}  // namespace interface
