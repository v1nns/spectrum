#include "view/block/tab_item/audio_equalizer.h"

#include "util/logger.h"

namespace interface {

AudioEqualizer::AudioEqualizer(const std::shared_ptr<EventDispatcher>& dispatcher)
    : TabItem(dispatcher), bars_{}, btn_apply_{nullptr}, btn_reset_{nullptr} {
  // Fill vector of frequency bars for equalizer
  std::vector<model::AudioFilter> filters{model::AudioFilter::Create()};
  bars_.reserve(filters.size());

  for (auto& filter : filters) {
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
        }
      },
      false);

  btn_reset_ = Button::make_button(
      std::string("Reset"),
      [&]() {
        LOG("Handle left click mouse event on Equalizer reset button");
        // Fill vector of frequency bars and send to player
        std::vector<model::AudioFilter> frequencies;
        frequencies.reserve(bars_.size());

        // Reset gain in all frequency bars
        for (auto& bar : bars_) {
          bar->ResetGain();
          frequencies.push_back(bar->GetAudioFilter());
        }

        auto dispatcher = dispatcher_.lock();
        if (dispatcher) {
          auto event = interface::CustomEvent::ApplyAudioFilters(frequencies);
          dispatcher->SendEvent(event);
          btn_apply_->SetInactive();
          btn_reset_->SetInactive();
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

  for (auto& bar : bars_) {
    if (bar->OnEvent(event)) {
      // TODO: set inactive when all gains are zero
      model::AudioFilter filter = bar->GetAudioFilter();
      if (filter.gain != 0) {
        btn_apply_->SetActive();
        btn_reset_->SetActive();
      }

      return true;
    }
  }

  return false;
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnCustomEvent(const CustomEvent& event) { return false; }

}  // namespace interface