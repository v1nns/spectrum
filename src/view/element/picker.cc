#include "view/element/picker.h"

#include <algorithm>
#include <utility>

#include "util/logger.h"
#include "view/element/style.h"

namespace interface {

Picker::Picker(const std::string& title, const keybinding::Key& key) : title_{title}, key_{key} {}

/* ********************************************************************************************** */

ftxui::Element Picker::Render() {
  const auto& colors = GetTheme().picker;

  // Descriptions (if any) are aligned, as names do not have the same length
  size_t name_length = 0;
  bool has_description = false;

  for (const auto& entry : entries_) {
    name_length = std::max(name_length, entry.name.size());
    has_description = has_description || !entry.description.empty();
  }

  ftxui::Elements entries;
  boxes_.resize(entries_.size());

  for (size_t i = 0; i < entries_.size(); i++) {
    const bool selected = i == selected_;
    const auto& entry = entries_[i];

    std::string padding(name_length - entry.name.size(), ' ');

    auto name = ftxui::text((selected ? "▶ " : "  ") + entry.name + padding +
                            (has_description ? "  " : " "));

    // When entry is wider than terminal, only description is cut (so names stay aligned)
    auto description =
        ftxui::text(entry.description.empty() ? "" : entry.description + " ") | ftxui::flex_shrink;

    auto content = selected
                       ? ftxui::hbox({name | ftxui::bold, description}) |
                             ftxui::color(colors.entry_selected) | ftxui::focus
                       : ftxui::hbox({name, description | ftxui::dim}) | ftxui::color(colors.entry);

    entries.push_back(content | ftxui::reflect(boxes_[i]));
  }

  // Frame keeps selected entry visible when there is not enough space for all of them (otherwise,
  // picker takes only the height needed for its entries and border). It scrolls only vertically,
  // as entries may be wider than terminal and their names must stay visible
  const int max_height = static_cast<int>(entries.size()) + 2;

  return ftxui::window(ftxui::text(" " + title_ + " ") | ftxui::color(colors.entry_selected),
                       ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::yframe) |
         ftxui::color(colors.border) | ftxui::bgcolor(GetTheme().screen.background) |
         ftxui::clear_under | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, max_height) |
         ftxui::reflect(Box());
}

/* ********************************************************************************************** */

bool Picker::OnEvent(const ftxui::Event& event) {
  if (!IsVisible()) return false;

  return HandleEvent(event) || IsModal();
}

/* ********************************************************************************************** */

bool Picker::HandleEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (event.is_mouse()) {
    ftxui::Event mouse_event = event;
    return OnMouseEvent(mouse_event);
  }

  // Move selection
  if (bool next = event == Keybind::ArrowDown || event == Keybind::Down;
      next || event == Keybind::ArrowUp || event == Keybind::Up) {
    Move(next);
    return true;
  }

  // Choose selected entry
  if (event == Keybind::Return || event == key_) {
    Choose();
    return true;
  }

  // Close picker without choosing anything
  if (event == Keybind::Escape || event == Keybind::Close) {
    LOG("Cancel picker=", title_);
    visible_ = false;
    OnCancel();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void Picker::Select(size_t index) {
  if (index == selected_) return;

  selected_ = index;
  OnSelect(index);
}

/* ********************************************************************************************** */

void Picker::Move(bool next) {
  if (next && selected_ + 1 < entries_.size()) Select(selected_ + 1);
  if (!next && selected_ > 0) Select(selected_ - 1);
}

/* ********************************************************************************************** */

void Picker::Choose() {
  visible_ = false;
  OnChoose(selected_);
}

/* ********************************************************************************************** */

std::optional<size_t> Picker::GetEntryAt(const ftxui::Mouse& mouse) const {
  for (size_t i = 0; i < boxes_.size(); i++) {
    if (boxes_[i].Contain(mouse.x, mouse.y)) return i;
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

void Picker::HandleWheel(const ftxui::Mouse::Button& button) {
  Move(button == ftxui::Mouse::WheelDown);
}

/* ********************************************************************************************** */

void Picker::HandleClick(ftxui::Event& event) {
  if (auto index = GetEntryAt(event.mouse()); index) Select(*index);
}

/* ********************************************************************************************** */

void Picker::HandleDoubleClick(ftxui::Event& event) {
  auto index = GetEntryAt(event.mouse());
  if (!index) return;

  Select(*index);
  Choose();
}

}  // namespace interface
