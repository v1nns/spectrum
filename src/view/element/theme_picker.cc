#include "view/element/theme_picker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "model/settings.h"
#include "util/logger.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

ThemePicker::ThemePicker(const std::shared_ptr<util::FileHandler>& file_handler)
    : file_handler_{file_handler} {
  const auto& themes = GetThemes();
  size_t index = 0;

  // Restore theme from last run (an unknown one falls back to default)
  if (model::Settings settings;
      file_handler_ && file_handler_->ParseSettings(settings) && settings.theme) {
    auto it = std::find_if(themes.begin(), themes.end(), [&settings](const ThemeOption& theme) {
      return theme.id == *settings.theme;
    });

    if (it != themes.end()) {
      index = static_cast<size_t>(it - themes.begin());
      INFO("Restored theme=", it->id);
    } else {
      WARN("Unknown theme in settings, using default one, theme=", *settings.theme);
    }
  }

  Apply(index);
}

/* ********************************************************************************************** */

ftxui::Element ThemePicker::Render() const {
  const auto& colors = GetTheme().picker;
  const auto& themes = GetThemes();

  ftxui::Elements entries;

  for (size_t i = 0; i < themes.size(); i++) {
    const bool selected = i == selected_;
    const std::string name{themes[i].name};

    auto entry = ftxui::text((selected ? "▶ " : "  ") + name + " ");
    entries.push_back(selected
                          ? entry | ftxui::bold | ftxui::color(colors.entry_selected) | ftxui::focus
                          : entry | ftxui::color(colors.entry));
  }

  // Frame keeps selected entry visible when there is not enough space for all of them (otherwise,
  // picker takes only the height needed for its entries and border)
  const int max_height = static_cast<int>(entries.size()) + 2;

  return ftxui::window(ftxui::text(" theme ") | ftxui::color(colors.entry_selected),
                       ftxui::vbox(std::move(entries)) | ftxui::vscroll_indicator | ftxui::frame) |
         ftxui::color(colors.border) | ftxui::clear_under |
         ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, max_height) | ftxui::center;
}

/* ********************************************************************************************** */

bool ThemePicker::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (!IsVisible()) return false;

  // Move selection, changing theme right away (so user can see it while choosing)
  if (bool next = event == Keybind::ArrowDown || event == Keybind::Down;
      next || event == Keybind::ArrowUp || event == Keybind::Up) {
    if (next && selected_ + 1 < GetThemes().size()) Apply(selected_ + 1);
    if (!next && selected_ > 0) Apply(selected_ - 1);
  }

  // Keep selected theme
  if (event == Keybind::Return || event == keybinding::General::ChangeTheme) {
    INFO("Selected theme=", GetThemes()[selected_].id);
    previous_.reset();
    SaveSettings();
  }

  // Go back to the theme from before opening picker
  if (event == Keybind::Escape || event == Keybind::Close) {
    LOG("Cancel theme picker");
    Apply(*previous_);
    previous_.reset();
  }

  // Picker is shown over all blocks, so do not let them handle anything while it is open
  return true;
}

/* ********************************************************************************************** */

void ThemePicker::Open() { previous_ = selected_; }

/* ********************************************************************************************** */

void ThemePicker::Apply(size_t index) {
  const auto& theme = GetThemes().at(index);
  LOG("Change theme to ", theme.id);

  selected_ = index;
  SetTheme(theme.colors);
}

/* ********************************************************************************************** */

void ThemePicker::SaveSettings() const {
  if (!file_handler_) return;

  model::Settings settings{.theme = std::string{GetThemes()[selected_].id}};
  if (!file_handler_->SaveSettings(settings)) ERROR("Cannot save theme");
}

}  // namespace interface
