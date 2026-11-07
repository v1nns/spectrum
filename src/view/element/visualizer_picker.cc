#include "view/element/visualizer_picker.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "util/logger.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

namespace {

//! Get next/previous value from an option, going around after the last one (or before the first)
template <typename T>
T Cycle(T value, T first, T last, bool next) {
  const int count = static_cast<int>(last) - static_cast<int>(first);
  const int index = static_cast<int>(value) - static_cast<int>(first);
  return static_cast<T>(static_cast<int>(first) + ((index + (next ? 1 : count - 1)) % count));
}

//! Name for an option that can only be enabled or disabled
std::string_view GetName(bool enabled) { return enabled ? "On" : "Off"; }

//! Every name that an option value may have (so all entries take the same width)
std::vector<std::string_view> GetAllNames() {
  std::vector<std::string_view> names{GetName(true), GetName(false)};

  for (int i = model::BarAnimation::HorizontalMirror; i < model::BarAnimation::LAST; i++) {
    names.push_back(model::GetAnimationName(static_cast<model::BarAnimation>(i)));
  }

  for (int i = 0; i < static_cast<int>(model::BarStyle::LAST); i++) {
    names.push_back(model::GetName(static_cast<model::BarStyle>(i)));
  }

  for (int i = 0; i < static_cast<int>(model::BarColors::LAST); i++) {
    names.push_back(model::GetName(static_cast<model::BarColors>(i)));
  }

  for (int i = 0; i < static_cast<int>(model::HueCycle::LAST); i++) {
    names.push_back(model::GetName(static_cast<model::HueCycle>(i)));
  }

  return names;
}

}  // namespace

/* ********************************************************************************************** */

VisualizerPicker::VisualizerPicker(const Callback& on_preview, const Callback& on_keep)
    : Picker("visualizer", keybinding::Visualizer::ChangeAnimation),
      on_preview_{on_preview},
      on_keep_{on_keep} {
  UpdateEntries();
}

/* ********************************************************************************************** */

void VisualizerPicker::Open(const model::VisualizerOptions& current) {
  options_ = current;
  previous_ = current;

  SetSelected(static_cast<size_t>(Option::Animation));
  UpdateEntries();
  Show();
}

/* ********************************************************************************************** */

void VisualizerPicker::OnSelect(size_t /*index*/) { UpdateEntries(); }

/* ********************************************************************************************** */

void VisualizerPicker::OnChoose(size_t /*index*/) { on_keep_(options_); }

/* ********************************************************************************************** */

void VisualizerPicker::OnCancel() {
  options_ = previous_;
  UpdateEntries();
  on_preview_(previous_);
}

/* ********************************************************************************************** */

bool VisualizerPicker::OnKey(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (bool next = event == Keybind::ArrowRight || event == Keybind::Right;
      next || event == Keybind::ArrowLeft || event == Keybind::Left) {
    Change(next);
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void VisualizerPicker::HandleWheel(const ftxui::Mouse::Button& button) {
  Change(button == ftxui::Mouse::WheelDown);
}

/* ********************************************************************************************** */

void VisualizerPicker::Change(bool next) {
  auto option = static_cast<Option>(GetSelected());
  if (!IsEnabled(option)) return;

  switch (option) {
    case Option::Animation:
      options_.animation = Cycle(options_.animation, model::BarAnimation::HorizontalMirror,
                                 model::BarAnimation::LAST, next);
      break;
    case Option::Bars:
      options_.bars = Cycle(options_.bars, model::BarStyle::Solid, model::BarStyle::LAST, next);
      break;
    case Option::Spacing:
      options_.spacing = !options_.spacing;
      break;
    case Option::Colors:
      options_.colors =
          Cycle(options_.colors, model::BarColors::Vertical, model::BarColors::LAST, next);
      break;
    case Option::Peaks:
      options_.peaks = !options_.peaks;
      break;
    case Option::Hue:
      options_.hue = Cycle(options_.hue, model::HueCycle::Off, model::HueCycle::LAST, next);
      break;
    case Option::LAST:
      return;
  }

  LOG("Preview visualizer options, animation=", model::GetAnimationName(options_.animation),
      " bars=", model::GetName(options_.bars), " spacing=", options_.spacing,
      " colors=", model::GetName(options_.colors), " peaks=", options_.peaks,
      " hue=", model::GetName(options_.hue));

  UpdateEntries();
  on_preview_(options_);
}

/* ********************************************************************************************** */

bool VisualizerPicker::IsEnabled(Option option) const {
  switch (option) {
    case Option::Bars:
    case Option::Spacing:
    case Option::Peaks:
      return model::IsBarAnimation(options_.animation);

    case Option::Hue:
      // Zones and single color from theme do not use gradient, so there is nothing to rotate
      return options_.colors != model::BarColors::Zones && !GetTheme().visualizer.solid;

    case Option::Animation:
    case Option::Colors:
    case Option::LAST:
      break;
  }

  return true;
}

/* ********************************************************************************************** */

void VisualizerPicker::UpdateEntries() {
  static const size_t kValueWidth = [] {
    const auto names = GetAllNames();
    return std::max_element(names.begin(), names.end(), [](auto a, auto b) {
             return a.size() < b.size();
           })->size();
  }();

  struct Content {
    std::string_view name;
    std::string_view value;
  };

  const std::array<Content, static_cast<size_t>(Option::LAST)> contents{{
      {"Animation", model::GetAnimationName(options_.animation)},
      {"Bars", model::GetName(options_.bars)},
      {"Spacing", GetName(options_.spacing)},
      {"Colors", model::GetName(options_.colors)},
      {"Peak caps", GetName(options_.peaks)},
      {"Hue cycle", model::GetName(options_.hue)},
  }};

  std::vector<Entry> entries;

  for (size_t i = 0; i < contents.size(); i++) {
    const auto& [name, value] = contents[i];
    const bool selected = i == GetSelected();

    // Value is padded, so picker keeps the same width while values change
    const std::string padding(kValueWidth - value.size(), ' ');

    entries.push_back(Entry{
        .name = std::string{name},
        .description = (selected ? "◂ " : "  ") + std::string{value} + (selected ? " ▸" : "  ") +
                       padding,
        .disabled = !IsEnabled(static_cast<Option>(i)),
    });
  }

  SetEntries(std::move(entries));
}

}  // namespace interface
