#include "view/block/main_content/audio_equalizer.h"

#include <functional>

#include "ftxui/dom/elements.hpp"
#include "model/settings.h"
#include "util/logger.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"
#include "view/element/util.h"

namespace interface {

AudioEqualizer::AudioEqualizer(const model::BlockIdentifier& id,
                               const std::shared_ptr<EventDispatcher>& dispatcher,
                               const FocusCallback& on_focus, const keybinding::Key& keybinding,
                               const std::shared_ptr<util::FileHandler>& file_handler)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string(kTabName)),
      file_handler_{file_handler} {
  // Initialize picker
  picker_.Initialize(presets_, &preset_name_, &last_applied_.genre,
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

  RestoreSettings();
}

/* ********************************************************************************************** */

ftxui::Element AudioEqualizer::Render() {
  ftxui::Element margin = ftxui::text(std::string(kMarginColumns, ' '));

  auto separator = [] {
    return ftxui::text(std::string{kSeparator}) | ftxui::color(GetTheme().equalizer.label);
  };

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
          btn_preset_->Render(),
          separator(),
          btn_apply_->Render(),
          separator(),
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
  // Focus and open preset picker
  if (event == keybinding::Equalizer::OpenPresets) {
    LOG("Handle key to open preset picker");

    // While list is opened, same key works like Return (as in any other picker), choosing the
    // preset highlighted and closing list
    if (picker_.opened) {
      focus_ctl_.SetFocus(picker_);
      return OnEvent(keybinding::Navigation::Return);
    }

    btn_preset_->OnClick();
    return true;
  }

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

  if (OnNavigationEvent(event)) return true;

  // Pass event to focus controller to handle and pass it along to focused element
  if (focus_ctl_.OnEvent(event)) {
    UpdateButtonState();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool AudioEqualizer::OnMouseEvent(ftxui::Event& event) {
  // List of presets is closed by a click on anything else (which is not handled by what is
  // behind it, except for buttons)
  const bool list_closed = picker_.CloseOnClickOutside(event);

  // Its button only opens list (a click on it while list is opened just closes it, like a click
  // on anything else)
  if (!list_closed && btn_preset_->OnMouseEvent(event)) return true;

  if (btn_apply_->OnMouseEvent(event)) return true;

  if (btn_reset_->OnMouseEvent(event)) return true;

  if (list_closed || focus_ctl_.OnMouseEvent(event)) {
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

bool AudioEqualizer::OnNavigationEvent(const ftxui::Event& event) {
  using keybinding::Navigation;

  const bool vertical = event == Navigation::ArrowUp || event == Navigation::Up ||
                        event == Navigation::ArrowDown || event == Navigation::Down;

  const bool bar_focused = std::any_of(bars_.begin(), bars_.end(),
                                       [](const FrequencyBar& bar) { return bar.IsFocused(); });

  // While nothing is focused, up/down start from preset picker (as it is above frequency bars)
  if (!vertical || picker_.IsFocused() || bar_focused) return false;

  LOG("Handle navigation key=", util::EventToString(event));
  focus_ctl_.SetFocus(picker_);
  return true;
}

/* ********************************************************************************************** */

void AudioEqualizer::CreateButtons() {
  auto style = Button::Style{
      .colors = [] { return GetTheme().equalizer.button; },
  };

  btn_preset_ = Button::make_button_hint(
      "e", "preset",
      [this]() {
        LOG("Handle callback for Equalizer preset button");
        OpenPresetPicker();
        return true;
      },
      style);

  btn_apply_ = Button::make_button_hint(
      "a", "apply",
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
        SaveSettings();

        // Set this block as active (focused)
        if (on_focus_) on_focus_();

        return true;
      },
      style);

  btn_reset_ = Button::make_button_hint(
      "r", "reset",
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Equalizer reset button");

        // Update buttons state
        btn_apply_->Disable();
        btn_reset_->Disable();

        // Selected preset is not the one applied, so go back to it
        if (preset_name_ != last_applied_.genre) {
          UpdatePreset(last_applied_.genre);
          UpdateButtonState();

          // Set this block as active (focused)
          if (on_focus_) on_focus_();

          return true;
        }

        if (preset_name_ != kModifiablePreset) return false;
        auto& current = current_preset();

        // Reset current EQ
        std::transform(current.begin(), current.end(), current.begin(),
                       [](model::AudioFilter& filter) {
                         filter.gain = 0;
                         return filter;
                       });

        // Gains are saved even when there is nothing to send (as they were not applied yet)
        SaveSettings();

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
        SaveSettings();

        // Set this block as active (focused)
        if (on_focus_) on_focus_();

        return true;
      },
      style);

  // Nothing to apply or reset until something is changed
  btn_apply_->Disable();
  btn_reset_->Disable();
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
  // - current preset is not the one applied, or
  // - current preset is "Custom" and exists at least one bar with gain different from zero
  if (preset_name_ != last_applied_.genre ||
      (preset_name_ == kModifiablePreset &&
       std::any_of(current.begin(), current.end(),
                   [](const model::AudioFilter& filter) { return filter.gain != 0; }))) {
    btn_reset_->Enable();
  } else {
    btn_reset_->Disable();
  }
}

/* ********************************************************************************************** */

void AudioEqualizer::RestoreSettings() {
  model::Settings settings;
  if (!file_handler_ || !file_handler_->ParseSettings(settings)) return;

  // Gains from the only preset that may be modified
  if (settings.equalizer_custom) {
    auto& custom = presets_.find(model::MusicGenre(kModifiablePreset))->second;
    const auto& gains = *settings.equalizer_custom;

    for (size_t i = 0; i < custom.size() && i < gains.size(); i++) {
      custom[i].SetNormalizedGain(gains[i]);
    }
  }

  // Preset applied on last run (an unknown one is ignored)
  if (settings.equalizer_preset && presets_.find(*settings.equalizer_preset) != presets_.end()) {
    UpdatePreset(*settings.equalizer_preset);
  }

  // Nothing to apply when it is the same that audio player starts with
  if (const auto& current = current_preset(); last_applied_ != current) {
    INFO("Restored equalizer preset=", preset_name_);

    if (auto dispatcher = dispatcher_.lock(); dispatcher) {
      dispatcher->SendEvent(interface::CustomEvent::ApplyAudioFilters(current));
    }
  }

  last_applied_.Update(preset_name_, current_preset());
  UpdateButtonState();
}

/* ********************************************************************************************** */

void AudioEqualizer::SaveSettings() const {
  if (!file_handler_) return;

  const auto& custom = presets_.find(model::MusicGenre(kModifiablePreset))->second;
  std::vector<double> gains;

  for (const auto& filter : custom) gains.push_back(filter.gain);

  if (!file_handler_->SaveSettings(model::Settings{.equalizer_preset = last_applied_.genre,
                                                   .equalizer_custom = std::move(gains)})) {
    ERROR("Cannot save equalizer settings");
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

void AudioEqualizer::OpenPresetPicker() {
  focus_ctl_.SetFocus(picker_);
  if (!picker_.opened) picker_.Open();

  // Set this block as active (focused)
  if (on_focus_) on_focus_();
}

/* ********************************************************************************************** */

void AudioEqualizer::UpdatePreset(const model::MusicGenre& preset) {
  // Update preset and link new EQ settings to frequency bars
  preset_name_ = preset;
  LinkPresetToInterface(current_preset());
}

}  // namespace interface
