
#include "ui/block/list_directory.h"

#include <ctype.h>  // for tolower

#include <algorithm>   // for for_each, search, sort
#include <filesystem>  // for path, directory_iterator, opera...
#include <memory>      // for shared_ptr, make_unique, alloca...
#include <utility>     // for move

#include "ftxui/component/event.hpp"  // for Event, Event::ArrowDown, Event:...
#include "ftxui/component/mouse.hpp"  // for Mouse, Mouse::Left, Mouse::Whee...
#include "ftxui/dom/node.hpp"         // for Node
#include "ftxui/screen/color.hpp"     // for Color, Color::Green, Color::White

namespace interface {

/* ********************************************************************************************** */
/**
 * @brief Create a new custom style for Menu Entry
 *
 * @param c Color
 * @return MenuEntryOption Custom object with the requested color
 */
MenuEntryOption Colored(ftxui::Color c) {
  return MenuEntryOption{
      .style_normal = Decorator(color(c)),
      .style_focused = Decorator(color(c)) | inverted,
      .style_selected = Decorator(color(c)),
      .style_selected_focused = Decorator(color(c)) | inverted,
  };
}

//! Similar to std::clamp, but allow hi to be lower than lo.
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

/* ********************************************************************************************** */

ListDirectory::ListDirectory(const std::string& optional_path)
    : curr_dir_(optional_path == "" ? std::filesystem::current_path()
                                    : std::filesystem::path(optional_path)),
      entries_(),
      selected_(0),
      focused_(0),
      style_dir_(Colored(Color::Green)),
      style_file_(Colored(Color::White)),
      boxes_(),
      box_(),
      mode_search_({}) {
  RefreshList(curr_dir_);
}

/* ********************************************************************************************** */

Element ListDirectory::Render() {
  Clamp();
  Elements elements;
  bool is_menu_focused = Focused();

  int* selected = GetSelected();
  int* focused = GetFocused();

  for (int i = 0; i < Size(); ++i) {
    bool is_focused = (*focused == i) && is_menu_focused;
    bool is_selected = (*selected == i);

    auto& entry = GetEntry(i);
    auto& type = entry.is_dir ? style_dir_ : style_file_;
    auto icon = is_selected ? "> " : "  ";

    auto style = is_selected ? (is_focused ? type.style_selected_focused : type.style_selected)
                             : (is_focused ? type.style_focused : type.style_normal);

    auto focus_management = is_focused ? ftxui::select : nothing;

    elements.push_back(text(icon + entry.path) | style | focus_management | reflect(boxes_[i]));
  }

  auto curr_dir_title = text(curr_dir_.c_str()) | bold;

  auto search_box = mode_search_ ? text("Text to search: " + mode_search_->text_to_search)
                                 : std::make_unique<Node>();

  return window(text(" Files "), vbox({
                                     hbox(std::move(curr_dir_title)),
                                     vbox(std::move(elements)) | reflect(box_) | frame | flex,
                                     hbox(std::move(search_box)),
                                 }));
}

/* ********************************************************************************************** */

bool ListDirectory::OnEvent(Event event) {
  Clamp();

  if (event.is_mouse()) {
    return OnMouseEvent(event);
  }

  if (Focused()) {
    if (OnMenuNavigation(event)) {
      return true;
    }

    if (mode_search_ && OnSearchModeEvent(event)) {
      return true;
    }

    // Enable search mode
    if (!mode_search_ && event == Event::Character('/')) {
      mode_search_ = Search({
          .text_to_search = "",
          .entries = entries_,
          .selected = 0,
          .focused = 0,
      });
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

  int* selected = GetSelected();
  int* focused = GetFocused();

  for (int i = 0; i < Size(); ++i) {
    if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) continue;

    TakeFocus();
    *focused = i;
    if (event.mouse().button == Mouse::Left && event.mouse().motion == Mouse::Released) {
      // Mouse click on menu entry
      if (*selected != i) *selected = i;
      return true;
    }
  }

  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnMouseWheel(Event event) {
  if (!box_.Contain(event.mouse().x, event.mouse().y)) {
    return false;
  }

  int* selected = GetSelected();
  int* focused = GetFocused();

  int old_selected = *selected;

  if (event.mouse().button == Mouse::WheelUp) {
    (*selected)--;
    (*focused)--;
  }

  if (event.mouse().button == Mouse::WheelDown) {
    (*selected)++;
    (*focused)++;
  }

  *selected = clamp(*selected, 0, Size() - 1);
  *focused = clamp(*focused, 0, Size() - 1);

  return true;
}

/* ********************************************************************************************** */

bool ListDirectory::OnMenuNavigation(Event event) {
  bool event_handled = false;
  int* selected = GetSelected();
  int* focused = GetFocused();

  int old_selected = *selected;

  if (event == Event::ArrowUp || event == Event::Character('k')) (*selected)--;
  if (event == Event::ArrowDown || event == Event::Character('j')) (*selected)++;
  if (event == Event::PageUp) (*selected) -= box_.y_max - box_.y_min;
  if (event == Event::PageDown) (*selected) += box_.y_max - box_.y_min;
  if (event == Event::Home) (*selected) = 0;
  if (event == Event::End) (*selected) = Size() - 1;
  if (event == Event::Tab && Size()) *selected = (*selected + 1) % Size();
  if (event == Event::TabReverse && Size()) *selected = (*selected + Size() - 1) % Size();

  if (*selected != old_selected) {
    *selected = clamp(*selected, 0, Size() - 1);

    if (*selected != old_selected) {
      *focused = *selected;
      event_handled = true;
    }
  }

  // Otherwise, user may want to change current directory
  if (event == Event::Return) {
    std::filesystem::path new_dir;
    auto active = GetActiveEntry();

    if (active != nullptr) {
      if (active->path == ".." && std::filesystem::exists(curr_dir_.parent_path())) {
        new_dir = curr_dir_.parent_path();
      } else if (active->is_dir) {
        new_dir = curr_dir_ / active->path;
      }

      if (!new_dir.empty()) {
        RefreshList(new_dir);

        // Exit search mode if enabled
        mode_search_.reset();

        event_handled = true;
      }
    }
  }

  return event_handled;
}

/* ********************************************************************************************** */

bool ListDirectory::OnSearchModeEvent(Event event) {
  bool event_handled = false;

  // Any alphabetic character
  if (event.is_character()) {
    mode_search_->text_to_search += event.character();
    event_handled = true;
  }

  // Backspace
  if (event == Event::Backspace && !(mode_search_->text_to_search.empty())) {
    mode_search_->text_to_search.pop_back();
    event_handled = true;
  }

  // Ctrl + Backspace
  if (event == Event::Special({8}) || event == Event::Special("\027")) {
    mode_search_->text_to_search.clear();
    event_handled = true;
  }

  if (event_handled) {
    RefreshSearchList();
  }

  // Quit search mode
  if (event == Event::Escape) {
    mode_search_.reset();
    event_handled = true;
  }

  return event_handled;
}

/* ********************************************************************************************** */

void ListDirectory::Clamp() {
  boxes_.resize(Size());

  int* selected = GetSelected();
  int* focused = GetFocused();

  *selected = clamp(*selected, 0, Size() - 1);
  *focused = clamp(*focused, 0, Size() - 1);
}

/* ********************************************************************************************** */

void ListDirectory::RefreshList(const std::filesystem::path& dir_path) {
  if (curr_dir_ != dir_path) curr_dir_ = dir_path;
  entries_.clear();
  selected_ = 0, focused_ = 0;

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
  entries_.insert(entries_.begin(), File{.path = "..", .is_dir = true});
}

/* ********************************************************************************************** */

void ListDirectory::RefreshSearchList() {
  mode_search_->selected = 0, mode_search_->focused = 0;

  // Do not even try to find it in the main list
  if (mode_search_->text_to_search.empty()) {
    mode_search_->entries = entries_;
    return;
  }

  auto compare_string = [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); };

  mode_search_->entries.clear();
  auto& text_to_search = mode_search_->text_to_search;

  for (auto& entry : entries_) {
    auto it = std::search(entry.path.begin(), entry.path.end(), text_to_search.begin(),
                          text_to_search.end(), compare_string);

    if (it != entry.path.end()) {
      mode_search_->entries.push_back(entry);
    }
  }
}

}  // namespace interface
