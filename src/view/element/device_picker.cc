#include "view/element/device_picker.h"

#include <algorithm>
#include <iomanip>
#include <string>
#include <utility>

#include "model/settings.h"
#include "util/logger.h"
#include "view/base/custom_event.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

DevicePicker::DevicePicker(const std::shared_ptr<EventDispatcher>& dispatcher,
                           const std::shared_ptr<util::FileHandler>& file_handler)
    : dispatcher_{dispatcher}, file_handler_{file_handler} {
  model::Settings settings;

  // Device from last run is already in use by audio player (which got it from settings as well)
  if (file_handler_ && file_handler_->ParseSettings(settings) && settings.device) {
    current_ = *settings.device;
  }
}

/* ********************************************************************************************** */

ftxui::Element DevicePicker::Render() {
  const auto& colors = GetTheme().picker;

  // Descriptions are aligned, as names do not have the same length
  size_t name_length = 0;
  for (const auto& device : devices_) name_length = std::max(name_length, device.name.size());

  ftxui::Elements entries;
  boxes_.resize(devices_.size());

  for (size_t i = 0; i < devices_.size(); i++) {
    const bool selected = i == selected_;
    const auto& device = devices_[i];

    std::string padding(name_length - device.name.size(), ' ');

    auto name = ftxui::text((selected ? "▶ " : "  ") + device.name + padding + "  ");
    // When entry is wider than terminal, only description is cut (so names stay aligned)
    auto description = ftxui::text(device.description.empty() ? "" : device.description + " ") |
                       ftxui::flex_shrink;

    auto entry = selected
                     ? ftxui::hbox({name | ftxui::bold, description}) |
                           ftxui::color(colors.entry_selected) | ftxui::focus
                     : ftxui::hbox({name, description | ftxui::dim}) | ftxui::color(colors.entry);

    entries.push_back(entry | ftxui::reflect(boxes_[i]));
  }

  // Frame keeps selected entry visible when there is not enough space for all of them (otherwise,
  // picker takes only the height needed for its entries and border). It scrolls only vertically,
  // as entries may be wider than terminal and their names must stay visible
  const int max_height = static_cast<int>(entries.size()) + 2;

  return ftxui::window(ftxui::text(" audio output ") | ftxui::color(colors.entry_selected),
                       ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::yframe) |
         ftxui::color(colors.border) | ftxui::bgcolor(GetTheme().screen.background) |
         ftxui::clear_under | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, max_height) |
         ftxui::reflect(Box()) | ftxui::center;
}

/* ********************************************************************************************** */

bool DevicePicker::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (!IsVisible()) return false;

  // Picker is shown over all blocks, so mouse is not handled by them either (even outside picker)
  if (event.is_mouse()) {
    ftxui::Event mouse_event = event;
    OnMouseEvent(mouse_event);

    return true;
  }

  // Move selection (device is changed only when it is chosen, as it may interrupt song for a
  // moment)
  if (bool next = event == Keybind::ArrowDown || event == Keybind::Down;
      next || event == Keybind::ArrowUp || event == Keybind::Up) {
    Move(next);
  }

  // Use selected device
  if (event == Keybind::Return || event == keybinding::General::ChangeAudioDevice) Choose();

  // Keep device in use
  if (event == Keybind::Escape || event == Keybind::Close) {
    LOG("Cancel audio output device picker");
    visible_ = false;
  }

  // Picker is shown over all blocks, so do not let them handle anything while it is open
  return true;
}

/* ********************************************************************************************** */

void DevicePicker::Open(const model::AudioDevices& devices) {
  devices_.clear();
  devices_.push_back(model::AudioDevice{.name = "automatic",
                                        .description = "Default device from system (or the first "
                                                       "one available)"});
  devices_.insert(devices_.end(), devices.begin(), devices.end());

  // Device in use may not be available anymore (e.g. it was disconnected)
  auto it =
      std::find_if(devices_.begin() + 1, devices_.end(),
                   [this](const model::AudioDevice& device) { return device.name == current_; });

  selected_ = it != devices_.end() ? static_cast<size_t>(it - devices_.begin()) : 0;
  visible_ = true;
}

/* ********************************************************************************************** */

void DevicePicker::Apply() {
  // First entry is the one to not choose any device
  current_ = selected_ > 0 ? devices_[selected_].name : "";
  INFO("Selected audio output device=", std::quoted(current_));

  if (auto dispatcher = dispatcher_.lock(); dispatcher) {
    dispatcher->SendEvent(CustomEvent::SetAudioDevice(current_));
  }

  if (!file_handler_) return;

  model::Settings settings{.device = current_};
  if (!file_handler_->SaveSettings(settings)) ERROR("Cannot save audio output device");
}

/* ********************************************************************************************** */

void DevicePicker::Move(bool next) {
  if (next && selected_ + 1 < devices_.size()) selected_++;
  if (!next && selected_ > 0) selected_--;
}

/* ********************************************************************************************** */

void DevicePicker::Choose() {
  Apply();
  visible_ = false;
}

/* ********************************************************************************************** */

std::optional<size_t> DevicePicker::GetEntryAt(const ftxui::Mouse& mouse) const {
  for (size_t i = 0; i < boxes_.size(); i++) {
    if (boxes_[i].Contain(mouse.x, mouse.y)) return i;
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

void DevicePicker::HandleWheel(const ftxui::Mouse::Button& button) {
  Move(button == ftxui::Mouse::WheelDown);
}

/* ********************************************************************************************** */

void DevicePicker::HandleClick(ftxui::Event& event) {
  if (auto index = GetEntryAt(event.mouse()); index) selected_ = *index;
}

/* ********************************************************************************************** */

void DevicePicker::HandleDoubleClick(ftxui::Event& event) {
  auto index = GetEntryAt(event.mouse());
  if (!index) return;

  selected_ = *index;
  Choose();
}

}  // namespace interface
