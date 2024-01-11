#include "view/element/internal/file_menu.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {
namespace internal {

FileMenu::FileMenu(const TextAnimation::Callback& force_refresh, const Callback& on_click)
    : Menu(std::move(force_refresh)), on_click_{on_click} {}

/* ********************************************************************************************** */

ftxui::Element FileMenu::RenderImpl() {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  auto max_size = GetMaxColumns() ? ftxui::size(WIDTH, EQUAL, GetMaxColumns()) : ftxui::nothing;

  ftxui::Elements menu_entries;
  menu_entries.reserve(GetSize());

  const auto selected = GetSelected();
  const auto focused = GetFocused();
  auto& box = GetBox();
  auto& boxes = GetBoxes();

  // Fill list with entries
  for (int i = 0; i < GetSize(); ++i) {
    const auto& entry = IsSearchEnabled() ? filtered_entries_->at(i) : entries_.at(i);

    bool is_focused = (*focused == i);
    bool is_selected = (*selected == i);
    bool is_highlighted = highlighted_ && entry == *highlighted_;

    const auto& type = is_highlighted                         ? style_.playing
                       : std::filesystem::is_directory(entry) ? style_.directory
                                                              : style_.file;

    auto prefix = ftxui::text(is_selected ? "▶ " : "  ");

    ftxui::Decorator style = is_selected ? (is_focused ? type.selected_focused : type.selected)
                                         : (is_focused ? type.focused : type.normal);

    auto focus_management = is_focused ? ftxui::select : ftxui::nothing;

    // In case of entry text too long, animation thread will be running, so we gotta take the
    // text content from there
    auto text = ftxui::text(IsAnimationRunning() && is_selected ? GetTextFromAnimation()
                                                                : entry.filename().string());

    menu_entries.push_back(ftxui::hbox({
                               prefix | style_.prefix,
                               text | style | ftxui::xflex,
                           }) |
                           max_size | focus_management | ftxui::reflect(boxes[i]));
  }

  ftxui::Elements content{
      ftxui::vbox(menu_entries) | ftxui::reflect(box) | ftxui::frame | ftxui::flex,
  };

  // Append search box, if enabled
  if (IsSearchEnabled()) {
    const auto& search = GetSearch();
    ftxui::InputOption opt{.cursor_position = search->position};
    content.push_back(ftxui::hbox({
        ftxui::text("Search:") | ftxui::color(ftxui::Color::White),
        ftxui::Input(search->text_to_search, " ", &opt)->Render() | ftxui::flex,
    }));
  }

  return ftxui::vbox(content) | ftxui::flex;
}

/* ********************************************************************************************** */

bool FileMenu::OnEventImpl(const ftxui::Event& event) {
  // Enable search mode
  if (!IsSearchEnabled() && event == keybinding::Files::EnableSearch) {
    EnableSearch();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool FileMenu::OnClickImpl() {
  auto active = GetActiveEntryImpl();

  if (!active.has_value()) return false;

  return on_click_(*active);
}

/* ********************************************************************************************** */

void FileMenu::FilterEntriesBy(const std::string& text) {
  // Do not even try to find it in the main list
  if (text.empty()) {
    filtered_entries_ = entries_;
    return;
  }

  filtered_entries_->clear();

  // Filter entries
  for (const auto& entry : entries_) {
    if (util::contains(entry.filename().string(), text)) {
      filtered_entries_->push_back(entry);
    }
  }
}

/* ********************************************************************************************** */

void FileMenu::SetEntryHighlightedImpl(const util::File& entry) {
  // Find entry in internal list
  auto it = std::find(entries_.begin(), entries_.end(), entry);

  if (it == entries_.end()) {
    LOG("Could not find entry to highlight");
    return;
  }

  highlighted_ = entry;

  // To get a better experience, update focused and select indexes,
  // to highlight current playing song entry in list
  int index = static_cast<int>(it - entries_.begin());

  ResetState(index);
}
}  // namespace internal
}  // namespace interface
