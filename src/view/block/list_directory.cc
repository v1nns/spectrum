
#include "view/block/list_directory.h"

#include <ctype.h>  // for tolower

#include <algorithm>   // for for_each, search, sort
#include <filesystem>  // for path, directory_iterator
#include <iomanip>
#include <memory>   // for shared_ptr, __shared_p...
#include <utility>  // for move

#include "ftxui/component/component.hpp"       // for Input
#include "ftxui/component/component_base.hpp"  // for Component, ComponentBase
#include "ftxui/component/event.hpp"           // for Event
#include "ftxui/component/mouse.hpp"           // for Mouse
#include "ftxui/screen/color.hpp"              // for Color
#include "ftxui/util/ref.hpp"                  // for Ref
#include "util/logger.h"
#include "view/base/event_dispatcher.h"

namespace interface {

//! Create a new custom style for Menu Entry
MenuEntryOption Colored(ftxui::Color c) {
  using ftxui::Decorator, ftxui::color, ftxui::inverted;
  return MenuEntryOption{
      .normal = Decorator(color(c)),
      .focused = Decorator(color(c)) | inverted,
      .selected = Decorator(color(c)) | inverted,
      .selected_focused = Decorator(color(c)) | inverted,
  };
}

//! Similar to std::clamp, but allow hi to be lower than lo.
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

//! Convert ftxui::Event to an user-friendly string
static std::string EventToString(const ftxui::Event& e) {
  if (e.is_character()) return e.character();

  if (e == ftxui::Event::ArrowUp) return "ArrowUp";
  if (e == ftxui::Event::ArrowDown) return "ArrowDown";
  if (e == ftxui::Event::PageUp) return "PageUp";
  if (e == ftxui::Event::PageDown) return "PageDown";
  if (e == ftxui::Event::Home) return "Home";
  if (e == ftxui::Event::End) return "End";
  if (e == ftxui::Event::Tab) return "Tab";
  if (e == ftxui::Event::TabReverse) return "Shift+Tab";
  if (e == ftxui::Event::Return) return "Return";

  return "";
}

/* ********************************************************************************************** */

ListDirectory::ListDirectory(const std::shared_ptr<EventDispatcher>& dispatcher,
                             const std::string& optional_path)
    : Block{dispatcher, Identifier::ListDirectory,
            interface::Size{.width = kMaxColumns, .height = 0}},
      curr_dir_{optional_path == "" ? std::filesystem::current_path()
                                    : std::filesystem::path(optional_path)},
      curr_playing_{std::nullopt},
      entries_{},
      selected_{},
      focused_{},
      styles_{EntryStyles{.directory = std::move(Colored(ftxui::Color::Green)),
                          .file = std::move(Colored(ftxui::Color::White)),
                          .playing = std::move(Colored(ftxui::Color::SteelBlue1))}},
      boxes_{},
      box_{},
      mode_search_{std::nullopt},
      animation_{TextAnimation{.enabled = false}} {
  // TODO: this is not good, read this below
  // https://google.github.io/styleguide/cppguide.html#Doing_Work_in_Constructors
  RefreshList(curr_dir_);

  animation_.cb_update = [&] {
    // Send user action to controller
    auto dispatcher = dispatcher_.lock();
    if (dispatcher) {
      auto event = interface::CustomEvent::Refresh();
      dispatcher->SendEvent(event);
    }
  };
}

/* ********************************************************************************************** */

ListDirectory::~ListDirectory() {
  if (animation_.enabled) animation_.Stop();
}

/* ********************************************************************************************** */

ftxui::Element ListDirectory::Render() {
  using ftxui::WIDTH, ftxui::EQUAL;

  Clamp();
  ftxui::Elements entries;

  int* selected = GetSelected();
  int* focused = GetFocused();

  // Title
  ftxui::Element curr_dir_title = ftxui::text(GetTitle()) | ftxui::bold;

  // Fill list with entries
  for (int i = 0; i < Size(); ++i) {
    bool is_focused = (*focused == i);
    bool is_selected = (*selected == i);

    File& entry = GetEntry(i);
    auto& type = entry == curr_playing_                 ? styles_.playing
                 : std::filesystem::is_directory(entry) ? styles_.directory
                                                        : styles_.file;
    const char* icon = is_selected ? "> " : "  ";

    ftxui::Decorator style = is_selected ? (is_focused ? type.selected_focused : type.selected)
                                         : (is_focused ? type.focused : type.normal);

    auto focus_management = is_focused ? ftxui::select : ftxui::nothing;

    // In case of entry text too long, animation thread will be running, so we gotta take the text
    // content from there
    std::string text =
        animation_.enabled && is_selected ? animation_.text : entry.filename().string();

    entries.push_back(ftxui::text(icon + text) | ftxui::size(WIDTH, EQUAL, kMaxColumns) | style |
                      focus_management | ftxui::reflect(boxes_[i]));
  }

  // Build up the content
  ftxui::Elements content{
      ftxui::hbox(std::move(curr_dir_title)),
      ftxui::vbox(std::move(entries)) | ftxui::reflect(box_) | ftxui::frame | ftxui::flex,
  };

  // Append search box, if enabled
  if (mode_search_) {
    ftxui::InputOption opt{.cursor_position = mode_search_->position};
    ftxui::Element search_box = ftxui::hbox({
        ftxui::text("Search:"),
        ftxui::Input(&mode_search_->text_to_search, " ", &opt)->Render() | ftxui::flex,
    });

    content.push_back(std::move(search_box));
  }

  return ftxui::window(ftxui::text(" files "), ftxui::vbox(std::move(content)) | ftxui::flex |
                                                   ftxui::size(WIDTH, EQUAL, kMaxColumns));
}

/* ********************************************************************************************** */

bool ListDirectory::OnEvent(ftxui::Event event) {
  Clamp();

  if (event.is_mouse()) {
    return OnMouseEvent(event);
  }

  // if (Focused()) {
  if (mode_search_ && OnSearchModeEvent(event)) {
    return true;
  }

  if (Size() && OnMenuNavigation(event)) {
    return true;
  }

  // Enable search mode
  if (!mode_search_ && event == ftxui::Event::Character('/')) {
    LOG("Enable search mode");
    mode_search_ = Search({
        .text_to_search = "",
        .entries = entries_,
        .selected = 0,
        .focused = 0,
        .position = 0,
    });

    UpdateActiveEntry();
    return true;
  }
  // }
  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");

    // Exit search mode if enabled
    mode_search_.reset();

    // Set current song
    curr_playing_ = event.GetContent<model::Song>().filepath;
  }

  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    curr_playing_.reset();
  }

  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnMouseEvent(ftxui::Event event) {
  if (event.mouse().button == ftxui::Mouse::WheelDown ||
      event.mouse().button == ftxui::Mouse::WheelUp)
    return OnMouseWheel(event);

  if (event.mouse().button != ftxui::Mouse::Left && event.mouse().button != ftxui::Mouse::None)
    return false;

  if (!CaptureMouse(event)) return false;

  int* selected = GetSelected();
  int* focused = GetFocused();

  for (int i = 0; i < Size(); ++i) {
    if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) continue;

    TakeFocus();
    *focused = i;

    if (event.mouse().button == ftxui::Mouse::Left &&
        event.mouse().motion == ftxui::Mouse::Released) {
      LOG("Handle left click mouse event on entry=", i);
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

  LOG("Handle mouse wheel event");
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

  if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k'))
    *selected = (*selected + Size() - 1) % Size();
  if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j'))
    *selected = (*selected + 1) % Size();
  if (event == ftxui::Event::PageUp) (*selected) -= box_.y_max - box_.y_min;
  if (event == ftxui::Event::PageDown) (*selected) += box_.y_max - box_.y_min;
  if (event == ftxui::Event::Home) (*selected) = 0;
  if (event == ftxui::Event::End) (*selected) = Size() - 1;
  if (event == ftxui::Event::Tab) *selected = (*selected + 5) % Size();
  if (event == ftxui::Event::TabReverse) *selected = (*selected + Size() - 5) % Size();

  if (*selected != old_selected) {
    *selected = clamp(*selected, 0, Size() - 1);

    if (*selected != old_selected) {
      LOG("Handle menu navigation key=", EventToString(event));
      *focused = *selected;
      event_handled = true;

      UpdateActiveEntry();
    }
  }

  // Otherwise, user may want to change current directory
  if (event == ftxui::Event::Return) {
    std::filesystem::path new_dir;
    auto active = GetActiveEntry();

    if (active != nullptr) {
      LOG("Handle menu navigation key=", EventToString(event));

      if (active->filename() == ".." && std::filesystem::exists(curr_dir_.parent_path())) {
        new_dir = curr_dir_.parent_path();
      } else if (std::filesystem::is_directory(*active)) {
        new_dir = curr_dir_ / active->filename();
      } else {
        // Send user action to controller
        auto dispatcher = dispatcher_.lock();
        if (dispatcher) {
          auto event = interface::CustomEvent::NotifyFileSelection(*active);
          dispatcher->SendEvent(event);
        }
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
  bool event_handled = false, exit_from_search_mode = false;

  // Any alphabetic character
  if (event.is_character()) {
    mode_search_->text_to_search.insert(mode_search_->position, event.character());
    mode_search_->position++;
    event_handled = true;
  }

  // Backspace
  if (event == ftxui::Event::Backspace && !(mode_search_->text_to_search.empty())) {
    if (mode_search_->position > 0) {
      mode_search_->text_to_search.erase(mode_search_->position - 1, 1);
      mode_search_->position--;
    }
    event_handled = true;
  }

  // Ctrl + Backspace
  if (event == ftxui::Event::Special({8}) || event == ftxui::Event::Special("\027")) {
    mode_search_->text_to_search.clear();
    mode_search_->position = 0;
    event_handled = true;
  }

  // Arrow left
  if (event == ftxui::Event::ArrowLeft) {
    if (mode_search_->position > 0) mode_search_->position--;
    event_handled = true;
  }

  // Arrow right
  if (event == ftxui::Event::ArrowRight) {
    int size = mode_search_->text_to_search.size();
    if (mode_search_->position < size) mode_search_->position++;
    event_handled = true;
  }

  // Quit search mode
  if (event == ftxui::Event::Escape) {
    LOG("Exit from search mode");
    mode_search_.reset();
    event_handled = true;
    exit_from_search_mode = true;
  }

  if (event_handled) {
    if (!exit_from_search_mode) RefreshSearchList();

    UpdateActiveEntry();
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
  LOG("Refresh list with files from new directory=", std::quoted(dir_path.c_str()));
  Files tmp;

  try {
    // Add all files from the given directory
    for (auto const& entry : std::filesystem::directory_iterator(dir_path)) {
      tmp.emplace_back(entry);
    }
  } catch (std::exception& e) {
    ERROR("Cannot access directory, exception=", e.what());
    auto dispatcher = dispatcher_.lock();
    dispatcher->SetApplicationError(error::kAccessDirFailed);
    return;
  }

  if (curr_dir_ != dir_path) curr_dir_ = dir_path;
  entries_ = std::move(tmp);
  selected_ = 0, focused_ = 0;

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
  LOG("Refresh list on search mode");
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

/* ********************************************************************************************** */

void ListDirectory::UpdateActiveEntry() {
  // Stop animation thread
  if (animation_.enabled) animation_.Stop();

  if (Size() > 0) {
    // Check text length of active entry
    int* selected = GetSelected();
    std::string text = GetEntry(*selected).filename().string();
    int max_chars = text.length() + kMaxIconColumns;

    // Start animation thread
    if (max_chars > kMaxColumns) animation_.Start(text);
  }
}

}  // namespace interface
