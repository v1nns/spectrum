#include "view/element/device_picker.h"

#include <algorithm>
#include <iomanip>
#include <utility>
#include <vector>

#include "model/settings.h"
#include "util/logger.h"
#include "view/base/custom_event.h"
#include "view/base/keybinding.h"

namespace interface {

DevicePicker::DevicePicker(const std::shared_ptr<EventDispatcher>& dispatcher,
                           const std::shared_ptr<util::FileHandler>& file_handler)
    : Picker("audio output", keybinding::General::ChangeAudioDevice),
      dispatcher_{dispatcher},
      file_handler_{file_handler} {
  model::Settings settings;

  // Device from last run is already in use by audio player (which got it from settings as well)
  if (file_handler_ && file_handler_->ParseSettings(settings) && settings.device) {
    current_ = *settings.device;
  }
}

/* ********************************************************************************************** */

void DevicePicker::Open(const model::AudioDevices& devices) {
  // First entry is always the one to not choose any device (as audio player does it)
  std::vector<Entry> entries{
      Entry{.name = "automatic",
            .description = "Default device from system (or the first one available)"},
  };

  for (const auto& device : devices) {
    entries.push_back(Entry{.name = device.name, .description = device.description});
  }

  // Device in use may not be available anymore (e.g. it was disconnected)
  auto it = std::find_if(entries.begin() + 1, entries.end(),
                         [this](const Entry& entry) { return entry.name == current_; });

  SetSelected(it != entries.end() ? static_cast<size_t>(it - entries.begin()) : 0);
  SetEntries(std::move(entries));
  Show();
}

/* ********************************************************************************************** */

void DevicePicker::OnChoose(size_t index) {
  // First entry is the one to not choose any device
  current_ = index > 0 ? GetEntries().at(index).name : "";
  INFO("Selected audio output device=", std::quoted(current_));

  if (auto dispatcher = dispatcher_.lock(); dispatcher) {
    dispatcher->SendEvent(CustomEvent::SetAudioDevice(current_));
  }

  if (!file_handler_) return;

  model::Settings settings{.device = current_};
  if (!file_handler_->SaveSettings(settings)) ERROR("Cannot save audio output device");
}

}  // namespace interface
