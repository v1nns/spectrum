#include "view/block/main_content/audio_equalizer.h"

#include <functional>

#include "ftxui/dom/elements.hpp"
#include "util/logger.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"
#include "view/element/util.h"

namespace interface {

AudioEqualizer::AudioEqualizer(const model::BlockIdentifier& id,
                               const std::shared_ptr<EventDispatcher>& dispatcher,
                               const FocusCallback& on_focus, const keybinding::Key& keybinding)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string(kTabName)) {
  // Initialize picker
  picker_.Initialize(presets_, &preset_name_,
                     std::bind(&AudioEqualizer::UpdatePreset, this, std::placeholders::_1));

  // Link initial EQ settings to UI
  LinkPresetToInterface(current_preset());

  // Append both picker + frequency bar elements to have focus controlled by wrapper
  focus_ctl_.Append(picker_);
  focus_ctl_.Append(bars_.begin(), bars_.end());

  // When navigating into this tab, start focus on the first frequency bar (instead of the picker)
  focus_ctl_.SetInitialFocus(bars_.front());

  // Set zeroed custom EQ as last EQ applied
  last_applied_.Update(preset_name_, current_preset());

  // Initialize buttons
  CreateButtons();
}

/* ********************************************************************************************** */

ftxui::Element AudioEqualizer::Render() {
  ftxui::Element margin = ftxui::text(std::string(kMarginColumns, ' '));

  // Frequency bars, all of them with the same space in between
  ftxui::Elements bars;
  bars.reserve(bars_.size());

  for (auto& bar : bars_) bars.push_back(bar.Render());

  ftxui::Element content = ftxui::vbox({
      ftxui::text(""),
      ftxui::hbox({
          margin,
          picker_.Render(),
          ftxui::filler(),
          btn_apply_->Render(),
          ftxui::text(" "),
          btn_reset_->Render(),
          margin,
      }),
      ftxui::text(""),
      ftxui::hbox({
          margin,
          RenderScale(),
          spaced_row(std::move(bars)) | ftxui::xflex_grow,
          margin,
      }) | ftxui::yflex_grow,
      ftxui::text(""),
  });

  if (!picker_.opened) return content;

  // List of presets is shown above everything else, starting from the same place used by picker
  return ftxui::dbox({
      content,
      ftxui::vbox({
          ftxui::text(""),
          ftxui::hbox({margin, picker_.RenderOpened()}),
      }),
  });
}

/* ********************************************************************************************** */

ftxui::Element AudioEqualizer::RenderScale() const {
  // Maximum and minimum values for gain (e.g. "+12" and "-12")
  static const std::string kMaxGain =
      "+" + util::to_string_with_precision(model::AudioFilter::kMaxGain, 0);
  static const std::string kMinGain =
      util::to_string_with_precision(model::AudioFilter::kMinGain, 0);

  // Same lines used by a frequency bar, so each value is in the same line as the gain it means
  return ftxui::vbox({
             ftxui::text("Hz"),
             ftxui::text(""),
             ftxui::vbox({
                 ftxui::text(kMaxGain),
                 ftxui::filler(),
                 ftxui::text("0") | ftxui::align_right,
                 ftxui::filler(),
                 ftxui::text(kMinGain),
             }) | ftxui::yflex_grow,
             ftxui::text(""),
             ftxui::text("dB"),
         }) |
         ftxui::color(GetTheme().equalizer.label);
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnEvent(const ftxui::Event& event) {
  // Apply audio filters
  if (btn_apply_->IsActive() && event == keybinding::Equalizer::ApplyFilters) {
    LOG("Handle key to apply audio filters");
    btn_apply_->OnClick();
    return true;
  }

  // Reset audio filters
  if (btn_reset_->IsActive() && event == keybinding::Equalizer::ResetFilters) {
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

bool AudioEqualizer::OnMouseEvent(ftxui::Event& event) {
  if (btn_apply_->OnMouseEvent(event)) return true;

  if (btn_reset_->OnMouseEvent(event)) return true;

  if (focus_ctl_.OnMouseEvent(event)) {
    // Set focus on parent block, so keys go to equalizer after clicking on it
    if (on_focus_) on_focus_();

    UpdateButtonState();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnCustomEvent(const CustomEvent& event) { return false; }

/* ********************************************************************************************** */

void AudioEqualizer::CreateButtons() {
  auto style = Button::Style{
      .colors = [] { return GetTheme().equalizer.button; },
      .delimiters = Button::Delimiters{"[", "]"},
  };

  btn_apply_ = Button::make_button(
      "Apply",
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
        if (on_focus_) on_focus_();

        return true;
      },
      style, "A", false);

  btn_reset_ = Button::make_button(
      "Reset",
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
        if (on_focus_) on_focus_();

        return true;
      },
      style, "R", false);
}

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
