#include "ui/block/list_directory.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <utility>

namespace interface {

/* ********************************************************************************************** */
/**
 * @brief Custom colors for Menu Entry
 *
 * @param c Text color
 * @return MenuEntryOption Custom object with the requested color
 */
MenuEntryOption Colored(ftxui::Color c) {
  MenuEntryOption special_style;
  special_style.style_normal = Decorator(color(c));
  special_style.style_focused = Decorator(color(c)) | inverted;
  special_style.style_selected = Decorator(color(c));
  special_style.style_selected_focused = Decorator(color(c)) | inverted;
  return special_style;
}

// Similar to std::clamp, but allow hi to be lower than lo.
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

/* ********************************************************************************************** */

ListDirectory::ListDirectory()
    : curr_dir_(std::filesystem::current_path()), entries_({}), selected_(0), focused_(0) {
  RefreshList(curr_dir_);
}

/* ********************************************************************************************** */

Element ListDirectory::Render() {
  Clamp();
  Elements elements;
  bool is_menu_focused = Focused();
  for (int i = 0; i < size(); ++i) {
    bool is_focused = (focused_ == i) && is_menu_focused;
    bool is_selected = (selected_ == i);

    auto entry_option = entries_[i].is_dir ? Colored(Color::Green) : Colored(Color::White);

    auto style =
        is_selected
            ? (is_focused ? entry_option.style_selected_focused : entry_option.style_selected)
            : (is_focused ? entry_option.style_focused : entry_option.style_normal);
    // auto focus_management = !is_selected ? nothing : is_menu_focused ? focus : ftxui::select;
    auto focus_management = is_focused ? ftxui::select : nothing;

    elements.push_back(text(" " + entries_[i].path) | style | focus_management |
                       reflect(boxes_[i]));
  }
  return vbox({text(curr_dir_.c_str()) | bold, vbox(std::move(elements)) | reflect(box_) | frame}) |
         borderRounded;
}

/* ********************************************************************************************** */

bool ListDirectory::OnEvent(Event event) {
  Clamp();
  if (!CaptureMouse(event)) return false;

  if (event.is_mouse()) return OnMouseEvent(event);

  if (Focused()) {
    int old_selected = selected_;
    if (event == Event::ArrowUp || event == Event::Character('k')) (selected_)--;
    if (event == Event::ArrowDown || event == Event::Character('j')) (selected_)++;
    if (event == Event::PageUp) (selected_) -= box_.y_max - box_.y_min;
    if (event == Event::PageDown) (selected_) += box_.y_max - box_.y_min;
    if (event == Event::Home) (selected_) = 0;
    if (event == Event::End) (selected_) = size() - 1;
    if (event == Event::Tab && size()) selected_ = (selected_ + 1) % size();
    if (event == Event::TabReverse && size()) selected_ = (selected_ + size() - 1) % size();

    selected_ = clamp(selected_, 0, size() - 1);

    if (selected_ != old_selected) {
      focused_ = selected_;
      return true;
    }
  }

  if (event == Event::Return) {
    std::filesystem::path new_dir;
    auto& active = entries_.at(selected_);

    if (active.path == ".." && std::filesystem::exists(curr_dir_.parent_path())) {
      // Okay, it really exists, so it should go back one level
      new_dir = curr_dir_.parent_path();
    } else if (active.is_dir) {
      // If active item is a directory, should enter it
      new_dir = curr_dir_ / active.path;
    }

    if (!new_dir.empty()) {
      RefreshList(new_dir);
      return true;
    }
  }

  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnMouseEvent(Event event) {
  if (event.mouse().button == Mouse::WheelDown || event.mouse().button == Mouse::WheelUp) {
    return OnMouseWheel(event);
  }

  if (event.mouse().button != Mouse::None && event.mouse().button != Mouse::Left) {
    return false;
  }
  if (!CaptureMouse(event)) return false;
  for (int i = 0; i < size(); ++i) {
    if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) continue;

    TakeFocus();
    focused_ = i;
    if (event.mouse().button == Mouse::Left && event.mouse().motion == Mouse::Released) {
      if (selected_ != i) {
        selected_ = i;
      }
      return true;
    }
  }
  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnMouseWheel(Event event) {
  if (!box_.Contain(event.mouse().x, event.mouse().y)) return false;
  int old_selected = selected_;

  if (event.mouse().button == Mouse::WheelUp) {
    (selected_)--;
    (focused_)--;
  }
  if (event.mouse().button == Mouse::WheelDown) {
    (selected_)++;
    (focused_)++;
  }

  selected_ = clamp(selected_, 0, size() - 1);
  focused_ = clamp(focused_, 0, size() - 1);

  return true;
}

/* ********************************************************************************************** */

void ListDirectory::Clamp() {
  boxes_.resize(size());
  selected_ = clamp(selected_, 0, size() - 1);
  focused_ = clamp(focused_, 0, size() - 1);
}

/* ********************************************************************************************** */

void ListDirectory::RefreshList(const std::filesystem::path& dir_path) {
  if (curr_dir_ != dir_path) curr_dir_ = dir_path;
  entries_.clear();

  // Add all dir/file from the current directory
  for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
    entries_.emplace_back(File{
        .path = entry.path().filename(),
        .is_dir = entry.is_directory(),
    });
  }

  // Transform whole string into uppercase
  auto to_lower = [](char& c) { c = std::tolower(c); };

  // Created a custom file sort
  auto custom_sort = [&to_lower](const File& a, const File& b) {
    std::string lhs{a.path}, rhs{b.path};

    // Don't care if it is hidden (tried to make it similar to "ls" output)
    if (lhs.at(0) == '.') lhs.erase(0, 1);
    if (rhs.at(0) == '.') rhs.erase(0, 1);

    std::for_each(lhs.begin(), lhs.end(), to_lower);
    std::for_each(rhs.begin(), rhs.end(), to_lower);

    return lhs < rhs;
  };

  // Sort list alphabetically (case insensitive)
  std::sort(entries_.begin(), entries_.end(), custom_sort);

  // Add option to go back one level
  entries_.insert(entries_.begin(), File{
                                        .path = "..",
                                        .is_dir = true,
                                    });
}

}  // namespace interface
