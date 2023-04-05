#include "view/block/tab_item/audio_equalizer.h"

#include <algorithm>

#include "util/formatter.h"
#include "util/logger.h"

namespace interface {

AudioEqualizer::AudioEqualizer(const model::BlockIdentifier& id,
                               const std::shared_ptr<EventDispatcher>& dispatcher)
    : TabItem(id, dispatcher) {
  // Link audio filters to UI frequency bar element
  for (int i = 0; i < model::equalizer::kPresetSize; i++) {
    bars_[i].filter = &eq_custom_[i];
  }

  btn_apply_ = Button::make_button(
      std::string("Apply"),
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Equalizer apply button");

        // Do nothing if they are equal
        if (eq_custom_ == eq_last_applied_) return false;

        // Otherwise, send updated values to Audio Player
        auto event_filters = interface::CustomEvent::ApplyAudioFilters(eq_custom_);
        disp->SendEvent(event_filters);
        btn_apply_->SetInactive();

        // Update cache
        eq_last_applied_ = eq_custom_;

        // Set this block as active (focused)
        auto event_focus = interface::CustomEvent::SetFocused(TabItem::parent_id_);
        disp->SendEvent(event_focus);

        return true;
      },
      false);

  btn_reset_ = Button::make_button(
      std::string("Reset"),
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Equalizer reset button");

        // Update buttons state
        btn_apply_->SetInactive();
        btn_reset_->SetInactive();

        // Reset current EQ
        std::transform(eq_custom_.begin(), eq_custom_.end(), eq_custom_.begin(),
                       [](model::AudioFilter& filter) {
                         filter.gain = 0;
                         return filter;
                       });

        // Do nothing if all frequencies contains gain equal to zero
        if (bool all_zero =
                std::all_of(eq_last_applied_.begin(), eq_last_applied_.end(),
                            [](model::AudioFilter& filter) { return filter.gain == 0; });
            all_zero) {
          return false;
        }

        // Otherwise, send updated values to Audio Player
        auto event_filters = interface::CustomEvent::ApplyAudioFilters(eq_custom_);
        disp->SendEvent(event_filters);

        // Update cache
        eq_last_applied_ = eq_custom_;

        // Set this block as active (focused)
        auto event_focus = interface::CustomEvent::SetFocused(TabItem::parent_id_);
        disp->SendEvent(event_focus);

        return true;
      },
      false);
}

/* ********************************************************************************************** */

ftxui::Element AudioEqualizer::Render() {
  ftxui::Elements frequencies;

  frequencies.reserve(2 * bars_.size() + 1);
  frequencies.push_back(ftxui::filler());

  // Iterate through all frequency bars
  for (auto& bar : bars_) {
    frequencies.push_back(bar.Render());
    frequencies.push_back(ftxui::filler());
  }

  return ftxui::vbox(ftxui::hbox(frequencies) | ftxui::flex_grow,
                     ftxui::hbox(btn_apply_->Render(), btn_reset_->Render()) | ftxui::center);
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnEvent(const ftxui::Event& event) {
  if (OnNavigationEvent(event)) {
    return true;
  }

  // Remove focus state from frequency bar
  if (event == ftxui::Event::Escape) {
    if (elem_focused_ == kInvalidIndex) return false;

    LOG("Handle menu navigation key=", util::EventToString(event));

    // Invalidate old index for focused
    UpdateFocus(elem_focused_, kInvalidIndex);
    UpdateButtonState();

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

bool AudioEqualizer::OnMouseEvent(const ftxui::Event& event) {
  if (btn_apply_->OnEvent(event)) return true;

  if (btn_reset_->OnEvent(event)) return true;

  // Iterate through all frequency bars and pass event, if event is handled, update UI state
  if (bool event_handled =
          std::any_of(bars_.begin(), bars_.end(),
                      [&event](FrequencyBar& bar) { return bar.OnMouseEvent(event); });
      event_handled) {
    UpdateButtonState();
    // TODO: Send event for setting focus on parent block (AskForFocus)
    // Maybe create onclick callback for frequency bars
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnCustomEvent(const CustomEvent& event) { return false; }

/* ********************************************************************************************** */

bool AudioEqualizer::OnNavigationEvent(const ftxui::Event& event) {
  // Navigate on frequency bars
  if (event == ftxui::Event::ArrowRight || event == ftxui::Event::Character('l')) {
    LOG("Handle menu navigation key=", util::EventToString(event));

    // Calculate new index based on upper bound
    int new_value = elem_focused_ + (elem_focused_ < (static_cast<int>(bars_.size()) - 1) ? 1 : 0);
    UpdateFocus(elem_focused_, new_value);

    return true;
  }

  // Navigate on frequency bars
  if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::Character('h')) {
    LOG("Handle menu navigation key=", util::EventToString(event));

    // Calculate new index based on lower bound
    int new_value = elem_focused_ - (elem_focused_ > (kInvalidIndex + 1) ? 1 : 0);
    UpdateFocus(elem_focused_, new_value);

    return true;
  }

  // Change gain on frequency bar focused
  if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
    if (elem_focused_ == kInvalidIndex) return false;

    LOG("Handle menu navigation key=", util::EventToString(event));

    // Increment value and update UI
    double gain = bars_[elem_focused_].filter->gain + 1;
    bars_[elem_focused_].filter->SetNormalizedGain(gain);

    UpdateButtonState();

    return true;
  }

  // Change gain on frequency bar focused
  if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
    if (elem_focused_ == kInvalidIndex) return false;

    LOG("Handle menu navigation key=", util::EventToString(event));

    // Increment value and update UI
    double gain = bars_[elem_focused_].filter->gain - 1;
    bars_[elem_focused_].filter->SetNormalizedGain(gain);

    UpdateButtonState();

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void AudioEqualizer::UpdateButtonState() {
  // Set apply button as active only if current filters are different from cache
  if (eq_last_applied_ != eq_custom_) {
    btn_apply_->SetActive();
  } else {
    btn_apply_->SetInactive();
  }

  // Set reset button as active only if exists at least one bar with gain different from zero
  if (bool all_zero = std::all_of(eq_custom_.begin(), eq_custom_.end(),
                                  [](model::AudioFilter& filter) { return filter.gain == 0; });
      !all_zero) {
    btn_reset_->SetActive();
  } else {
    btn_reset_->SetInactive();
  }
}

/* ********************************************************************************************** */

void AudioEqualizer::UpdateFocus(int old_index, int new_index) {
  // If equal, do nothing
  if (old_index == new_index) return;

  // Remove focus from old focused frequency bar
  if (old_index != kInvalidIndex) bars_[old_index].ResetFocus();

  // Set focus on newly-focused frequency bar
  if (new_index != kInvalidIndex) bars_[new_index].SetFocus();

  // Update internal index
  elem_focused_ = new_index;
}

}  // namespace interface