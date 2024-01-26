#include "view/element/internal/file_menu.h"

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {
namespace internal {

FileMenu::FileMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                   const TextAnimation::Callback& force_refresh, const Callback& on_click)
    : Menu(dispatcher, force_refresh), on_click_{on_click} {}

/* ********************************************************************************************** */

ftxui::Element FileMenu::RenderImpl() {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  auto max_size = GetMaxColumns() ? ftxui::size(WIDTH, EQUAL, GetMaxColumns()) : ftxui::nothing;

  ftxui::Elements menu_entries;

  int size = GetSize();
  menu_entries.reserve(size);

  const auto selected = GetSelected();
  const auto focused = GetFocused();
  auto& box = GetBox();
  auto& boxes = GetBoxes();

  // Fill list with entries
  for (int i = 0; i < size; ++i) {
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
    content.push_back(RenderSearch());
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

int FileMenu::GetSizeImpl() const {
  int size = IsSearchEnabled() ? (int)filtered_entries_->size() : (int)entries_.size();
  return size;
}

/* ********************************************************************************************** */

std::string FileMenu::GetActiveEntryAsTextImpl() const {
  auto active = GetActiveEntryImpl();
  return active.has_value() ? active->filename().string() : "";
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

void FileMenu::SetEntriesImpl(const util::Files& entries) {
  LOG("Set a new list of entries with size=", entries.size());
  entries_ = entries;
}

/* ********************************************************************************************** */

void FileMenu::SetEntryHighlightedImpl(const util::File& entry) {
  // Find entry in internal list
  auto it = std::find(entries_.begin(), entries_.end(), entry);

  if (it == entries_.end()) {
    LOG("Could not find entry to highlight");
    return;
  }

  highlighted_ = *it;

  // To get a better experience, update focused and select indexes,
  // to highlight current playing song entry in list
  int index = static_cast<int>(it - entries_.begin());

  ResetState(index);
}

/* ********************************************************************************************** */

std::optional<util::File> FileMenu::GetActiveEntryImpl() const {
  int size = GetSizeImpl();

  // Empty list
  if (!size) return std::nullopt;

  // Get active entry
  std::optional<util::File> entry = std::nullopt;
  int index = GetSelected();

  // Check for boundary and if vector not empty
  if (index >= size || entries_.empty() ||
      (filtered_entries_.has_value() && filtered_entries_->empty()))
    return entry;

  entry = IsSearchEnabled() ? filtered_entries_->at(index) : entries_.at(index);
  return entry;
}

}  // namespace internal
}  // namespace interface
