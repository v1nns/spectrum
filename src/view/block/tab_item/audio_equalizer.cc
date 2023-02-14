#include "view/block/tab_item/audio_equalizer.h"

#include <algorithm>

#include "util/logger.h"

namespace interface {

AudioEqualizer::AudioEqualizer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : TabItem(dispatcher),
      cache_{model::AudioFilter::Create()},
      bars_{},
      btn_apply_{nullptr},
      btn_reset_{nullptr} {
  // Fill vector of frequency bars for equalizer
  bars_.reserve(cache_.size());

  for (auto& filter : cache_) {
    bars_.push_back(std::make_unique<FrequencyBar>(filter));
  }

  btn_apply_ = Button::make_button(
      std::string("Apply"),
      [&]() {
        LOG("Handle left click mouse event on Equalizer apply button");
        auto dispatcher = dispatcher_.lock();
        if (dispatcher) {
          // Fill vector of frequency bars and send to player
          std::vector<model::AudioFilter> frequencies;
          frequencies.reserve(bars_.size());

          for (const auto& bar : bars_) {
            frequencies.push_back(bar->GetAudioFilter());
          }

          auto event = interface::CustomEvent::ApplyAudioFilters(frequencies);
          dispatcher->SendEvent(event);
          btn_apply_->SetInactive();

          // Update cache
          cache_ = frequencies;
        }
      },
      false);

  btn_reset_ = Button::make_button(
      std::string("Reset"),
      [&]() {
        LOG("Handle left click mouse event on Equalizer reset button");
        auto dispatcher = dispatcher_.lock();
        if (dispatcher) {
          // Fill vector of frequency bars and send to player
          std::vector<model::AudioFilter> frequencies;
          frequencies.reserve(bars_.size());

          // Reset gain in all frequency bars
          for (auto& bar : bars_) {
            bar->ResetGain();
            frequencies.push_back(bar->GetAudioFilter());
          }

          auto event = interface::CustomEvent::ApplyAudioFilters(frequencies);
          dispatcher->SendEvent(event);
          btn_apply_->SetInactive();
          btn_reset_->SetInactive();

          // Update cache
          cache_ = frequencies;
        }
      },
      false);
}

/* ********************************************************************************************** */

ftxui::Element AudioEqualizer::Render() {
  ftxui::Elements frequencies;

  frequencies.push_back(ftxui::filler());

  // Iterate through all frequency bars
  for (const auto& bar : bars_) {
    frequencies.push_back(bar->Render());
    frequencies.push_back(ftxui::filler());
  }

  return ftxui::vbox(ftxui::hbox(frequencies) | ftxui::flex_grow,
                     ftxui::hbox(btn_apply_->Render(), btn_reset_->Render()) | ftxui::center);
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnEvent(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

bool AudioEqualizer::OnMouseEvent(ftxui::Event event) {
  if (btn_apply_->OnEvent(event)) return true;

  if (btn_reset_->OnEvent(event)) return true;

  bool bar_modified = false;

  // Iterate through all frequency bars and pass event
  for (auto& bar : bars_) {
    if (bar->OnEvent(event)) {
      bar_modified = true;
      break;
    }
  }

  // Event has been handled, so update UI state
  if (bar_modified) UpdateInterfaceState();

  return bar_modified;
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnCustomEvent(const CustomEvent& event) { return false; }

/* ********************************************************************************************** */

void AudioEqualizer::UpdateInterfaceState() {
  // Control flag for all gains zeroed
  bool all_zero = true;

  // Dummy structure to compare current values with cache
  std::vector<model::AudioFilter> current;
  current.reserve(bars_.size());

  // Iterate through all bars to check if it has some value set for gain
  for (auto& bar : bars_) {
    // Get associated filter to frequency bar
    auto filter = bar->GetAudioFilter();

    // Check value for gain
    if (filter.gain != 0) all_zero = false;

    // Add filter to dummy vector
    current.push_back(filter);
  }

  // Set apply button as active only if current filters are different from cache
  if (current != cache_) {
    btn_apply_->SetActive();
  } else {
    btn_apply_->SetInactive();
  }

  // Set reset button as active only if exists at least one bar with gain different from zero
  if (!all_zero) {
    btn_reset_->SetActive();
  } else {
    btn_reset_->SetInactive();
  }
}

}  // namespace interface