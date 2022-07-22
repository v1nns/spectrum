#include "view/block/audio_visualizer.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
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

  auto my_graph = [this](int width, int height) {
    std::vector<int> output(width);
    for (int i = 0; i < width; ++i) {
      // float v = 0.5f;
      float v = 0;
      if (i < data_.size()) {
        // v += data_.at(i);
        v += 0.1f * sin((i)*data_.at(i));
        // v += 0.2f * sin((i + 10) * 0.15f);
        // v += 0.1f * sin((i)*0.03f);
        v *= height;
      }

      output[i] = (int)v;
    }
    return output;
  };

  return ftxui::window(ftxui::text(" visualizer "), ftxui::graph(my_graph));
}

/* ********************************************************************************************** */

bool AudioVisualizer::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

bool AudioVisualizer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Type::DrawAudioRaw) {
    const auto& data = event.GetContent<std::vector<int>>();

    // TODO: do something with data
    data_ = std::move(data);

    return true;
  }

  return false;
}

}  // namespace interface
