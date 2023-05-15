#include "view/block/tab_item/audio_equalizer.h"

#include <functional>
namespace interface {

AudioEqualizer::AudioEqualizer(const model::BlockIdentifier& id,
                               const std::shared_ptr<EventDispatcher>& dispatcher)
    : TabItem(id, dispatcher) {
  // Initialize picker
  picker_.Initialize(presets_, &preset_name_,
                     std::bind(&AudioEqualizer::UpdatePreset, this, std::placeholders::_1));

  // Link initial EQ settings to UI
  LinkPresetToInterface(current_preset());

  // Append both picker + frequency bar elements to have focus controlled by wrapper
  focus_ctl_.Append(picker_);
  focus_ctl_.Append(bars_.begin(), bars_.end());

  // Set zeroed custom EQ as last EQ applied
  last_applied_.Update(preset_name_, current_preset());

  btn_apply_ = Button::make_button(
      std::string("Apply"),
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Equalizer apply button");
        const auto& current = current_preset();

        // Do nothing if they are equal
        if (last_applied_ == current) return false;

        // Otherwise, send updated values to Audio Player
        auto event_filters = interface::CustomEvent::ApplyAudioFilters(current);
        disp->SendEvent(event_filters);
        btn_apply_->Disable();

        // Update cache
        last_applied_.Update(preset_name_, current);

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
        btn_apply_->Disable();
        btn_reset_->Disable();

        if (preset_name_ != kModifiablePreset) return false;
        auto& current = current_preset();

        // Reset current EQ
        std::transform(current.begin(), current.end(), current.begin(),
                       [](model::AudioFilter& filter) {
                         filter.gain = 0;
                         return filter;
                       });

        // Do nothing if all frequencies contains gain equal to zero
        if (bool all_zero =
                std::all_of(last_applied_.preset.begin(), last_applied_.preset.end(),
                            [](const model::AudioFilter& filter) { return filter.gain == 0; });
            all_zero) {
          return false;
        }

        // // Otherwise, send updated values to Audio Player
        auto event_filters = interface::CustomEvent::ApplyAudioFilters(current);
        disp->SendEvent(event_filters);

        // Update cache
        last_applied_.Update(preset_name_, current);

        // Set this block as active (focused)
        auto event_focus = interface::CustomEvent::SetFocused(TabItem::parent_id_);
        disp->SendEvent(event_focus);

        return true;
      },
      false);
}

/* ********************************************************************************************** */

ftxui::Element AudioEqualizer::Render() {
  ftxui::Elements elements;

  // EQ picker + frequency bars
  elements.reserve(3 + 2 * bars_.size());

  elements.push_back(ftxui::filler());
  elements.push_back(picker_.Render());
  elements.push_back(ftxui::filler());

  // Iterate through all frequency bars
  for (auto& bar : bars_) {
    elements.push_back(bar.Render());
    elements.push_back(ftxui::filler());
  }

  return ftxui::vbox({
      ftxui::hbox(elements) | ftxui::flex_grow,
      ftxui::hbox(btn_apply_->Render(), btn_reset_->Render()) | ftxui::center,
  });
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnEvent(const ftxui::Event& event) {
  // Apply audio filters
  if (btn_apply_->IsActive() && event == ftxui::Event::Character('a')) {
    LOG("Handle key to apply audio filters");
    btn_apply_->OnClick();
    return true;
  }

  // Reset audio filters
  if (btn_reset_->IsActive() && event == ftxui::Event::Character('r')) {
    LOG("Handle key to reset audio filters");
    btn_reset_->OnClick();
    return true;
  }

  // Pass event to focus controller to handle and pass it along to focused element
  if (focus_ctl_.OnEvent(event)) {
    UpdateButtonState();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnMouseEvent(const ftxui::Event& event) {
  if (btn_apply_->OnMouseEvent(event)) return true;

  if (btn_reset_->OnMouseEvent(event)) return true;

  if (focus_ctl_.OnMouseEvent(event)) {
    // TODO: Send event for setting focus on parent block (AskForFocus)
    UpdateButtonState();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnCustomEvent(const CustomEvent& event) { return false; }

/* ********************************************************************************************** */

void AudioEqualizer::UpdateButtonState() {
  const auto& current = current_preset();

  // Set apply button as active only if current filters are different from cache
  if (last_applied_ != current) {
    btn_apply_->Enable();
  } else {
    btn_apply_->Disable();
  }

  // Set reset button as active only if:
  // - current preset is "Custom"
  // - exists at least one bar with gain different from zero
  if (preset_name_ == kModifiablePreset &&
      std::any_of(current.begin(), current.end(),
                  [](const model::AudioFilter& filter) { return filter.gain != 0; })) {
    btn_reset_->Enable();
  } else {
    btn_reset_->Disable();
  }
}

/* ********************************************************************************************** */

void AudioEqualizer::LinkPresetToInterface(model::EqualizerPreset& preset) {
  // Link audio filters to UI frequency bar element
  for (int i = 0; i < model::equalizer::kFiltersPerPreset; i++) {
    bars_[i].filter = &preset[i];
  }
}

/* ********************************************************************************************** */

void AudioEqualizer::UpdatePreset(const model::MusicGenre& preset) {
  // Update preset and link new EQ settings to frequency bars
  preset_name_ = preset;
  LinkPresetToInterface(current_preset());
}

}  // namespace interface
