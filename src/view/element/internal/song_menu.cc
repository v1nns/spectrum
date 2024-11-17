#include "view/element/internal/song_menu.h"

namespace interface {
namespace internal {

SongMenu::SongMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                   const TextAnimation::Callback& force_refresh, const Callback& on_click)
    : BaseMenu(dispatcher, force_refresh), on_click_{on_click} {}

/* ********************************************************************************************** */

ftxui::Element SongMenu::RenderImpl() {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  auto max_size = GetMaxColumns() ? ftxui::size(WIDTH, EQUAL, GetMaxColumns()) : ftxui::nothing;

  ftxui::Elements menu_entries;

  int size = GetSize();
  menu_entries.reserve(size);

  const auto selected = GetSelected();
  const auto focused = GetFocused();
  auto& boxes = GetBoxes();

  // Fill list with entries
  for (int i = 0; i < size; ++i) {
    const auto& entry = IsSearchEnabled() ? filtered_entries_->at(i) : entries_.at(i);

    bool is_focused = (*focused == i);
    bool is_selected = (*selected == i);

    const auto& type = style_.entry;

    auto prefix = ftxui::text(is_selected ? "▶ " : "  ");

    ftxui::Decorator style = is_selected ? (is_focused ? type.selected_focused : type.selected)
                                         : (is_focused ? type.focused : type.normal);

    auto focus_management = is_focused ? ftxui::select : ftxui::nothing;

    // In case of entry text too long, animation thread will be running, so we gotta take the
    // text content from there
    auto text =
        ftxui::text(IsAnimationRunning() && is_selected ? GetTextFromAnimation()
                                                        : entry.filepath.filename().string());
    menu_entries.push_back(ftxui::hbox({
                               prefix | style_.prefix,
                               text | style | ftxui::xflex,
                           }) |
                           max_size | focus_management | ftxui::reflect(boxes[i]));
  }

  ftxui::Elements content{
      ftxui::vbox(menu_entries) | ftxui::reflect(Box()) | ftxui::frame | ftxui::flex,
  };

  // Append search box, if enabled
  if (IsSearchEnabled()) {
    content.push_back(RenderSearch());
  }

  return ftxui::vbox(content) | ftxui::flex;
}

/* ********************************************************************************************** */

bool SongMenu::OnEventImpl(const ftxui::Event& event) {
  // Enable search mode
  if (!IsSearchEnabled() && event == keybinding::Navigation::EnableSearch) {
    EnableSearch();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

int SongMenu::GetSizeImpl() const {
  int size = IsSearchEnabled() ? (int)filtered_entries_->size() : (int)entries_.size();
  return size;
}

/* ********************************************************************************************** */

std::string SongMenu::GetActiveEntryAsTextImpl() const {
  auto active = GetActiveEntryImpl();
  return active.has_value() ? active->filepath.filename().string() : "";
}

/* ********************************************************************************************** */

bool SongMenu::OnClickImpl() {
  auto active = GetActiveEntryImpl();

  if (!active.has_value()) return false;

  // Otherwise, it is a file, so send it to owner class
  return on_click_(*active);
}

/* ********************************************************************************************** */

void SongMenu::FilterEntriesBy(const std::string& text) {
  // Do not even try to find it in the main list
  if (text.empty()) {
    filtered_entries_ = entries_;
    return;
  }

  filtered_entries_->clear();

  // Filter entries
  for (const auto& entry : entries_) {
    if (util::contains(entry.filepath.filename().string(), text)) {
      filtered_entries_->push_back(entry);
    }
  }
}

/* ********************************************************************************************** */

void SongMenu::SetEntriesImpl(const std::vector<model::Song>& entries) {
  LOG("Set a new list of entries with size=", entries.size());
  entries_ = entries;
}

/* ********************************************************************************************** */

std::optional<model::Song> SongMenu::GetActiveEntryImpl() const {
  int size = GetSizeImpl();

  // Empty list
  if (!size) return std::nullopt;

  // Get active entry
  std::optional<model::Song> entry = std::nullopt;
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
