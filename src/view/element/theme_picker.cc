#include "view/element/theme_picker.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "model/settings.h"
#include "util/logger.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

ThemePicker::ThemePicker(const std::shared_ptr<util::FileHandler>& file_handler)
    : Picker("theme", keybinding::General::ChangeTheme), file_handler_{file_handler} {
  const auto& themes = GetThemes();
  size_t index = 0;

  std::vector<Entry> entries;
  for (const auto& theme : themes) entries.push_back(Entry{.name = std::string{theme.name}});

  SetEntries(std::move(entries));

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

  SetSelected(index);
  Apply(index);
}

/* ********************************************************************************************** */

void ThemePicker::Open() {
  previous_ = GetSelected();
  Show();
}

/* ********************************************************************************************** */

void ThemePicker::Apply(size_t index) const {
  const auto& theme = GetThemes().at(index);
  LOG("Change theme to ", theme.id);

  SetTheme(theme.colors);
}

/* ********************************************************************************************** */

void ThemePicker::OnSelect(size_t index) { Apply(index); }

/* ********************************************************************************************** */

void ThemePicker::OnChoose(size_t index) {
  const auto& theme = GetThemes().at(index);
  INFO("Selected theme=", theme.id);

  if (!file_handler_) return;

  model::Settings settings{.theme = std::string{theme.id}};
  if (!file_handler_->SaveSettings(settings)) ERROR("Cannot save theme");
}

/* ********************************************************************************************** */

void ThemePicker::OnCancel() {
  SetSelected(previous_);
  Apply(previous_);
}

}  // namespace interface
