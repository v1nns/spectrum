#include "view/block/tab_item/audio_equalizer.h"

#include <algorithm>

#include "util/formatter.h"
#include "util/logger.h"

namespace interface {

AudioEqualizer::AudioEqualizer(const model::BlockIdentifier& id,
                               const std::shared_ptr<EventDispatcher>& dispatcher)
    : TabItem(id, dispatcher),
      cache_{model::AudioFilter::Create()},
      bars_{},
      btn_apply_{nullptr},
      btn_reset_{nullptr},
      focused_{kInvalidIndex} {
  // Fill vector of frequency bars for equalizer
  bars_.reserve(cache_.size());

  for (auto& filter : cache_) {
    bars_.push_back(std::make_unique<FrequencyBar>(filter));
  }

  btn_apply_ = Button::make_button(
      std::string("Apply"),
      [&]() -> bool {
        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) return false;

        LOG("Handle callback for Equalizer apply button");
        // Fill vector of frequency bars and send to player
        std::vector<model::AudioFilter> frequencies;
        frequencies.reserve(bars_.size());

        for (const auto& bar : bars_) {
          frequencies.push_back(bar->GetAudioFilter());
        }

        // Do nothing if they are equal
        if (cache_ == frequencies) return false;

        // Otherwise, send updated values to Audio Player
        auto event_filters = interface::CustomEvent::ApplyAudioFilters(frequencies);
        dispatcher->SendEvent(event_filters);
        btn_apply_->SetInactive();

        // Update cache
        cache_ = frequencies;

        // Set this block as active (focused)
        auto event_focus = interface::CustomEvent::SetFocused(TabItem::parent_id_);
        dispatcher->SendEvent(event_focus);

        return true;
      },
      false);

  btn_reset_ = Button::make_button(
      std::string("Reset"),
      [&]() -> bool {
        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) return false;

        LOG("Handle callback for Equalizer reset button");
        // Fill vector of frequency bars and send to player
        std::vector<model::AudioFilter> frequencies;
        frequencies.reserve(bars_.size());

        // Reset gain in all frequency bars
        for (auto& bar : bars_) {
          bar->ResetGain();
          frequencies.push_back(bar->GetAudioFilter());
        }

        // Update buttons state
        btn_apply_->SetInactive();
        btn_reset_->SetInactive();

        // Do nothing if they are equal
        if (cache_ == frequencies) return false;

        // Otherwise, send updated values to Audio Player
        auto event_filters = interface::CustomEvent::ApplyAudioFilters(frequencies);
        dispatcher->SendEvent(event_filters);

        // Update cache
        cache_ = frequencies;

        // Set this block as active (focused)
        auto event_focus = interface::CustomEvent::SetFocused(TabItem::parent_id_);
        dispatcher->SendEvent(event_focus);

        return true;
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

bool AudioEqualizer::OnEvent(ftxui::Event event) {
  // Navigate on frequency bars
  if (event == ftxui::Event::ArrowRight || event == ftxui::Event::Character('l')) {
    LOG("Handle menu navigation key=", util::EventToString(event));
    int old_index = focused_;

    // Calculate new index based on upper bound
    focused_ += focused_ < (static_cast<int>(bars_.size()) - 1) ? 1 : 0;

    if (old_index != focused_) {
      UpdateFocus(old_index);
    }

    return true;
  }

  // Navigate on frequency bars
  if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::Character('h')) {
    LOG("Handle menu navigation key=", util::EventToString(event));
    int old_index = focused_;

    // Calculate new index based on lower bound
    focused_ -= focused_ > (kInvalidIndex + 1) ? 1 : 0;

    if (old_index != focused_) {
      UpdateFocus(old_index);
    }

    return true;
  }

  // Change gain on frequency bar focused
  if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
    if (focused_ == kInvalidIndex) return false;

    LOG("Handle menu navigation key=", util::EventToString(event));

    // Increment value and update UI
    bars_[focused_]->IncreaseGain();
    UpdateInterfaceState();

    return true;
  }

  // Change gain on frequency bar focused
  if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
    if (focused_ == kInvalidIndex) return false;

    LOG("Handle menu navigation key=", util::EventToString(event));

    // Increment value and update UI
    bars_[focused_]->DecreaseGain();
    UpdateInterfaceState();

    return true;
  }

  // Remove focus state from frequency bar
  if (event == ftxui::Event::Escape) {
    if (focused_ == kInvalidIndex) return false;

    LOG("Handle menu navigation key=", util::EventToString(event));
    int old_index = focused_;

    // Invalidate old index for focused
    focused_ = kInvalidIndex;

    // Update UI
    UpdateFocus(old_index);
    UpdateInterfaceState();

    return true;
  }

  // Apply audio filters
  if (btn_apply_->IsActive() && event == ftxui::Event::Character('a')) {
    LOG("Handle key to apply audio filters");
    btn_apply_->OnClick();
  }

  // Reset audio filters
  if (btn_reset_->IsActive() && event == ftxui::Event::Character('r')) {
    LOG("Handle key to reset audio filters");
    btn_reset_->OnClick();
  }

  return false;
}

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
  if (bar_modified) {
    UpdateInterfaceState();
    // TODO: Send event for setting focus on parent block (AskForFocus)
    // Maybe create onclick callback for frequency bars
  }

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

/* ********************************************************************************************** */

void AudioEqualizer::UpdateFocus(int old_index) {
  // Remove focus from old focused frequency bar
  if (old_index != kInvalidIndex) bars_[old_index]->ResetFocus();

  // Set focus on newly-focused frequency bar
  if (focused_ != kInvalidIndex) bars_[focused_]->SetFocus();
}

}  // namespace interface