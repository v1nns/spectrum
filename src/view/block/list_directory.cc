
#include "view/block/list_directory.h"

#include <ctype.h>  // for tolower

#include <algorithm>   // for for_each, search, sort
#include <filesystem>  // for path, directory_iterator
#include <memory>      // for shared_ptr, __shared_p...
#include <utility>     // for move

#include "ftxui/component/component.hpp"       // for Input
#include "ftxui/component/component_base.hpp"  // for Component, ComponentBase
#include "ftxui/component/event.hpp"           // for Event
#include "ftxui/component/mouse.hpp"           // for Mouse
#include "ftxui/screen/color.hpp"              // for Color
#include "ftxui/util/ref.hpp"                  // for Ref
#include "view/base/event_dispatcher.h"

namespace interface {

/**
 * @brief Create a new custom style for Menu Entry
 *
 * @param c Color
 * @return MenuEntryOption Custom object with the requested color
 */
ftxui::MenuEntryOption Colored(ftxui::Color c) {
  using ftxui::Decorator, ftxui::color, ftxui::inverted;
  return ftxui::MenuEntryOption{
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

constexpr int kMaxColumns = 30;  //!< Maximum columns for the Component

/* ********************************************************************************************** */

ListDirectory::ListDirectory(const std::shared_ptr<EventDispatcher>& d,
                             const std::string& optional_path)
    : Block(d, kBlockListDirectory),
      curr_dir_(optional_path == "" ? std::filesystem::current_path()
                                    : std::filesystem::path(optional_path)),
      entries_(),
      selected_(0),
      focused_(0),
      styles_({.directory = std::move(Colored(ftxui::Color::Green)),
               .file = std::move(Colored(ftxui::Color::White)),
               .playing = std::move(Colored(ftxui::Color::Aquamarine1))}),
      boxes_(),
      box_(),
      mode_search_(std::nullopt) {
  // TODO: this is not good, read this below
  // https://google.github.io/styleguide/cppguide.html#Doing_Work_in_Constructors
  RefreshList(curr_dir_);
}

/* ********************************************************************************************** */

ftxui::Element ListDirectory::Render() {
  Clamp();
  ftxui::Elements entries;
  bool is_menu_focused = Focused();

  int* selected = GetSelected();
  int* focused = GetFocused();

  // Title
  ftxui::Element curr_dir_title = ftxui::text(GetTitle().c_str()) | ftxui::bold;

  // Fill list with entries
  for (int i = 0; i < Size(); ++i) {
    bool is_focused = (*focused == i) && is_menu_focused;
    bool is_selected = (*selected == i);

    File& entry = GetEntry(i);
    auto& type = std::filesystem::is_directory(entry) ? styles_.directory : styles_.file;
    const char* icon = is_selected ? "> " : "  ";

    ftxui::Decorator style = is_selected
                                 ? (is_focused ? type.style_selected_focused : type.style_selected)
                                 : (is_focused ? type.style_focused : type.style_normal);

    auto focus_management = is_focused ? ftxui::select : ftxui::nothing;

    entries.push_back(ftxui::text(icon + entry.filename().string()) | style | focus_management |
                      ftxui::reflect(boxes_[i]));
  }

  // Build up the content
  ftxui::Elements content{
      ftxui::hbox(std::move(curr_dir_title)),
      ftxui::vbox(std::move(entries)) | ftxui::reflect(box_) | ftxui::frame | ftxui::flex,
  };

  // Append search box, if enabled
  if (mode_search_) {
    ftxui::InputOption opt{.cursor_position = mode_search_->text_to_search.size()};
    ftxui::Element search_box = ftxui::hbox({
        ftxui::text("Search:"),
        ftxui::Input(&mode_search_->text_to_search, " ", &opt)->Render() | ftxui::inverted,
    });

    content.push_back(std::move(search_box));
  }
  using ftxui::WIDTH, ftxui::EQUAL;
  return ftxui::window(ftxui::text(" files "), ftxui::vbox(std::move(content)) | ftxui::flex |
                                                   ftxui::size(WIDTH, EQUAL, kMaxColumns));
}

/* ********************************************************************************************** */

bool ListDirectory::OnEvent(ftxui::Event event) {
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
    if (!mode_search_ && event == ftxui::Event::Character('/')) {
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

void ListDirectory::OnBlockEvent(BlockEvent event) {
  // TODO: do something in the future
}

/* ********************************************************************************************** */

bool ListDirectory::OnMouseEvent(ftxui::Event event) {
  if (event.mouse().button == ftxui::Mouse::WheelDown ||
      event.mouse().button == ftxui::Mouse::WheelUp) {
    return OnMouseWheel(event);
  }

  if (event.mouse().button != ftxui::Mouse::None && event.mouse().button != ftxui::Mouse::Left) {
    return false;
  }

  if (!CaptureMouse(event)) return false;

  int* selected = GetSelected();
  int* focused = GetFocused();

  for (int i = 0; i < Size(); ++i) {
    if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) continue;

    TakeFocus();
    *focused = i;
    if (event.mouse().button == ftxui::Mouse::Left &&
        event.mouse().motion == ftxui::Mouse::Released) {
      // Mouse click on menu entry
      if (*selected != i) *selected = i;
      return true;
    }
  }

  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnMouseWheel(ftxui::Event event) {
  if (!box_.Contain(event.mouse().x, event.mouse().y)) {
    return false;
  }

  int* selected = GetSelected();
  int* focused = GetFocused();

  int old_selected = *selected;

  if (event.mouse().button == ftxui::Mouse::WheelUp) {
    (*selected)--;
    (*focused)--;
  }

  if (event.mouse().button == ftxui::Mouse::WheelDown) {
    (*selected)++;
    (*focused)++;
  }

  *selected = clamp(*selected, 0, Size() - 1);
  *focused = clamp(*focused, 0, Size() - 1);

  return true;
}

/* ********************************************************************************************** */

bool ListDirectory::OnMenuNavigation(ftxui::Event event) {
  bool event_handled = false;
  int* selected = GetSelected();
  int* focused = GetFocused();

  int old_selected = *selected;

  if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) (*selected)--;
  if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) (*selected)++;
  if (event == ftxui::Event::PageUp) (*selected) -= box_.y_max - box_.y_min;
  if (event == ftxui::Event::PageDown) (*selected) += box_.y_max - box_.y_min;
  if (event == ftxui::Event::Home) (*selected) = 0;
  if (event == ftxui::Event::End) (*selected) = Size() - 1;
  if (event == ftxui::Event::Tab && Size()) *selected = (*selected + 1) % Size();
  if (event == ftxui::Event::TabReverse && Size()) *selected = (*selected + Size() - 1) % Size();

  if (*selected != old_selected) {
    *selected = clamp(*selected, 0, Size() - 1);

    if (*selected != old_selected) {
      *focused = *selected;
      event_handled = true;
    }
  }

  // Otherwise, user may want to change current directory
  if (event == ftxui::Event::Return) {
    std::filesystem::path new_dir;
    auto active = GetActiveEntry();

    if (active != nullptr) {
      if (active->filename() == ".." && std::filesystem::exists(curr_dir_.parent_path())) {
        new_dir = curr_dir_.parent_path();
      } else if (std::filesystem::is_directory(*active)) {
        new_dir = curr_dir_ / active->filename();
      } else {
        // Send user action to controller
        auto listener = listener_.lock();

        // TODO: do something on else?
        if (listener) listener->NotifyFileSelection(*active);
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

bool ListDirectory::OnSearchModeEvent(ftxui::Event event) {
  bool event_handled = false;

  // Any alphabetic character
  if (event.is_character()) {
    mode_search_->text_to_search += event.character();
    event_handled = true;
  }

  // Backspace
  if (event == ftxui::Event::Backspace && !(mode_search_->text_to_search.empty())) {
    mode_search_->text_to_search.pop_back();
    event_handled = true;
  }

  // Ctrl + Backspace
  if (event == ftxui::Event::Special({8}) || event == ftxui::Event::Special("\027")) {
    mode_search_->text_to_search.clear();
    event_handled = true;
  }

  if (event_handled) {
    RefreshSearchList();
  }

  // Quit search mode
  if (event == ftxui::Event::Escape) {
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

std::string ListDirectory::GetTitle() {
  const std::string curr_dir = curr_dir_.string();

  // Everything fine, directory does not exceed maximum column length
  if (curr_dir.size() <= kMaxColumns) {
    return curr_dir;
  }

  // Oh no, it does exceed, so we must truncate the surplus text
  int offset = curr_dir.size() - (kMaxColumns - 5);  // Considering window border(2) + ellipsis(3)
  const std::string& substr = curr_dir.substr(offset);
  auto index = substr.find('/');

  return index != std::string::npos ? std::string("..." + substr.substr(index)) : substr;
}

/* ********************************************************************************************** */

void ListDirectory::RefreshList(const std::filesystem::path& dir_path) {
  if (curr_dir_ != dir_path) curr_dir_ = dir_path;
  entries_.clear();
  selected_ = 0, focused_ = 0;

  // Add all files from the given directory
  for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
    entries_.emplace_back(entry);
  }

  // Transform whole string into uppercase
  auto to_lower = [](char& c) { c = std::tolower(c); };

  // Created a custom file sort
  auto custom_sort = [&to_lower](const File& a, const File& b) {
    std::string lhs{a.filename()}, rhs{b.filename()};

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
  entries_.insert(entries_.begin(), File{".."});
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
    const std::string filename = entry.filename().string();
    auto it = std::search(filename.begin(), filename.end(), text_to_search.begin(),
                          text_to_search.end(), compare_string);

    if (it != filename.end()) {
      mode_search_->entries.push_back(entry);
    }
  }
}

}  // namespace interface
