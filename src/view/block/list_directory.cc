
#include "view/block/list_directory.h"

#include <ctype.h>  // for tolower

#include <algorithm>   // for for_each, search, sort
#include <filesystem>  // for path, directory_iterator
#include <ftxui/dom/elements.hpp>
#include <iomanip>
#include <memory>   // for shared_ptr, __shared_p...
#include <utility>  // for move

#include "ftxui/component/component.hpp"       // for Input
#include "ftxui/component/component_base.hpp"  // for Component, ComponentBase
#include "ftxui/component/event.hpp"           // for Event
#include "ftxui/component/mouse.hpp"           // for Mouse
#include "ftxui/screen/color.hpp"              // for Color
#include "ftxui/util/ref.hpp"                  // for Ref
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"

namespace interface {

//! Similar to std::clamp, but allow hi to be lower than lo.
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return v < lo ? lo : hi < v ? hi : v;
}

/* ********************************************************************************************** */

ListDirectory::ListDirectory(const std::shared_ptr<EventDispatcher>& dispatcher,
                             const std::string& optional_path)
    : Block{dispatcher, model::BlockIdentifier::ListDirectory,
            interface::Size{.width = kMaxColumns, .height = 0}} {
  // TODO: this is not good, read this below
  // https://google.github.io/styleguide/cppguide.html#Doing_Work_in_Constructors
  auto path = !optional_path.empty() ? std::filesystem::path(optional_path)
                                     : std::filesystem::current_path();

  if (bool parsed = RefreshList(path); !optional_path.empty() && !parsed) {
    // If we can't list files from current path, then everything is gone
    RefreshList(std::filesystem::current_path());
  }

  animation_.cb_update = [this] {
    // Send user action to controller
    auto disp = GetDispatcher();
    disp->SendEvent(interface::CustomEvent::Refresh());
  };
}

/* ********************************************************************************************** */

ListDirectory::~ListDirectory() { animation_.Stop(); }

/* ********************************************************************************************** */

ftxui::Element ListDirectory::Render() {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  Clamp();
  ftxui::Elements entries;
  entries.reserve(Size());

  const auto selected = GetSelected();
  const auto focused = GetFocused();

  // Title
  ftxui::Element curr_dir_title = ftxui::text(GetTitle()) | styles_.title;

  // Fill list with entries
  for (int i = 0; i < Size(); ++i) {
    bool is_focused = (*focused == i);
    bool is_selected = (*selected == i);

    const File& entry = GetEntry(i);
    const auto& type = entry == curr_playing_                 ? styles_.playing
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
      ftxui::hbox(curr_dir_title),
      ftxui::vbox(entries) | ftxui::reflect(box_) | ftxui::frame | ftxui::flex,
  };

  // Append search box, if enabled
  if (mode_search_) {
    ftxui::InputOption opt{.cursor_position = mode_search_->position};
    ftxui::Element search_box = ftxui::hbox({
        ftxui::text("Search:") | ftxui::color(ftxui::Color::White),
        ftxui::Input(&mode_search_->text_to_search, " ", &opt)->Render() | ftxui::flex,
    });

    content.push_back(search_box);
  }

  return ftxui::window(
             ftxui::hbox(ftxui::text(" files ") | GetTitleDecorator()),
             ftxui::vbox(content) | ftxui::flex | ftxui::size(WIDTH, EQUAL, kMaxColumns)) |
         GetBorderDecorator();
}

/* ********************************************************************************************** */

bool ListDirectory::OnEvent(ftxui::Event event) {
  Clamp();

  if (event.is_mouse()) {
    return OnMouseEvent(event);
  }

  // If block is not focused, do not even try to handle event
  if (!IsFocused()) {
    return false;
  }

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

  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");

    // Set current song
    curr_playing_ = event.GetContent<model::Song>().filepath;

    // Exit search mode if enabled
    if (mode_search_) {
      mode_search_.reset();

      // To get a better experience, update focused and select indexes,
      // to highlight current playing song entry in list
      auto it = std::find(entries_.begin(), entries_.end(), *curr_playing_);
      int index = it != entries_.end() ? static_cast<int>(it - entries_.begin()) : 0;

      focused_ = index;
      selected_ = index;
    }

    // Update active entry (to enable/disable text animation)
    UpdateActiveEntry();
  }

  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    curr_playing_.reset();
  }

  if (event == CustomEvent::Identifier::PlaySong) {
    LOG("Received request from media player to play selected file");
    auto dispatcher = GetDispatcher();

    auto active = GetActiveEntry();
    auto event_selection = interface::CustomEvent::NotifyFileSelection(*active);
    dispatcher->SendEvent(event_selection);

    return true;
  }

  if (event == CustomEvent::Identifier::SkipToPreviousSong) {
    LOG("Received request from media player to play previous song");

    if (auto file = SelectFileToPlay(/*pick_next=*/false); !file.empty()) {
      LOG("Skipping song, attempt to play previous file: ", file);
      // Send user action to controller
      auto dispatcher = GetDispatcher();
      auto event_selection = interface::CustomEvent::NotifyFileSelection(file);
      dispatcher->SendEvent(event_selection);
    }

    return true;
  }

  if (event == CustomEvent::Identifier::SkipToNextSong) {
    LOG("Received request from media player to play next song");

    if (auto file = SelectFileToPlay(/*pick_next=*/true); !file.empty()) {
      LOG("Skipping song, attempt to play next file: ", file);
      // Send user action to controller
      auto dispatcher = GetDispatcher();
      auto event_selection = interface::CustomEvent::NotifyFileSelection(file);
      dispatcher->SendEvent(event_selection);
    }

    return true;
  }

#ifndef SPECTRUM_DEBUG
  // Do not return true because other blocks may use it
  if (curr_playing_ && event == CustomEvent::Identifier::UpdateSongState) {
    // TODO: disable this attempt to play next song for the following situations:
    // - with SPECTRUM_DEBUG=ON
    // - when user stopped song (by stop button or S key)

    // In case that song has finished successfully, attempt to play next one
    if (auto content = event.GetContent<model::Song::CurrentInformation>();
        content.state == model::Song::MediaState::Finished) {
      if (auto file = SelectFileToPlay(/*pick_next=*/true); !file.empty()) {
        LOG("Song finished, attempt to play next file: ", file);
        // Send user action to controller
        auto dispatcher = GetDispatcher();
        auto event_selection = interface::CustomEvent::NotifyFileSelection(file);
        dispatcher->SendEvent(event_selection);
      }
    }
  }
#endif

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
      *selected = i;

      // Send event for setting focus on this block
      AskForFocus();
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

bool ListDirectory::OnMenuNavigation(const ftxui::Event& event) {
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

  if (*selected != old_selected) {
    *selected = clamp(*selected, 0, Size() - 1);

    if (*selected != old_selected) {
      LOG("Handle menu navigation key=", util::EventToString(event));
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
      LOG("Handle menu navigation key=", util::EventToString(event));

      if (active->filename() == ".." && std::filesystem::exists(curr_dir_.parent_path())) {
        new_dir = curr_dir_.parent_path();
      } else if (std::filesystem::is_directory(*active)) {
        new_dir = curr_dir_ / active->filename();
      } else {
        // Send user action to controller
        auto dispatcher = GetDispatcher();
        auto event_selection = interface::CustomEvent::NotifyFileSelection(*active);
        dispatcher->SendEvent(event_selection);
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

bool ListDirectory::OnSearchModeEvent(const ftxui::Event& event) {
  bool event_handled = false;
  bool exit_from_search_mode = false;

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
    if (auto size = (int)mode_search_->text_to_search.size(); mode_search_->position < size)
      mode_search_->position++;
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
  int offset =
      (int)curr_dir.size() - (kMaxColumns - 5);  // Considering window border(2) + ellipsis(3)
  const std::string& substr = curr_dir.substr(offset);
  auto index = substr.find('/');

  return index != std::string::npos ? std::string("..." + substr.substr(index)) : substr;
}

/* ********************************************************************************************** */

bool ListDirectory::RefreshList(const std::filesystem::path& dir_path) {
  LOG("Refresh list with files from new directory=", std::quoted(dir_path.c_str()));
  Files tmp;

  try {
    // Add all files from the given directory
    for (auto const& entry : std::filesystem::directory_iterator(dir_path)) {
      tmp.emplace_back(entry);
    }
  } catch (std::exception& e) {
    ERROR("Cannot access directory, exception=", e.what());
    auto dispatcher = GetDispatcher();
    dispatcher->SetApplicationError(error::kAccessDirFailed);
    return false;
  }

  // Reset internal values
  curr_dir_ = dir_path;
  entries_.swap(tmp);
  selected_ = 0;
  focused_ = 0;

  // Transform whole string into uppercase
  constexpr auto to_lower = [](char& c) { c = (char)std::tolower(c); };

  // Created a custom file sort
  auto custom_sort = [to_lower](const File& a, const File& b) {
    std::string lhs{a.filename()};
    std::string rhs{b.filename()};

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
  entries_.emplace(entries_.begin(), "..");
  return true;
}

/* ********************************************************************************************** */

void ListDirectory::RefreshSearchList() {
  LOG("Refresh list on search mode");
  mode_search_->selected = 0;
  mode_search_->focused = 0;

  // Do not even try to find it in the main list
  if (mode_search_->text_to_search.empty()) {
    mode_search_->entries = entries_;
    return;
  }

  auto compare_string = [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); };

  mode_search_->entries.clear();
  auto& text_to_search = mode_search_->text_to_search;

  for (const auto& entry : entries_) {
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
  animation_.Stop();

  if (Size() > 0) {
    // Check text length of active entry
    const auto selected = GetSelected();
    std::string text{GetEntry(*selected).filename().string().append(" ")};
    int max_chars = (int)text.length() + kMaxIconColumns;

    // Start animation thread
    if (max_chars > kMaxColumns) animation_.Start(text);
  }
}

/* ********************************************************************************************** */

File ListDirectory::SelectFileToPlay(bool is_next) {
  if (entries_.size() <= 2) return File{};

  // Get index from current song playing
  auto index = static_cast<int>(
      std::distance(entries_.begin(), std::find(entries_.begin(), entries_.end(), *curr_playing_)));

  auto size = static_cast<int>(entries_.size());

  int new_index = is_next ? (index + 1) % size : (index + size - 1) % size;
  int attempts = size;
  File file;

  // Iterate circularly through all file entries
  bool found = false;
  for (file = entries_[new_index]; attempts > 0; --attempts, file = entries_[new_index]) {
    // Found a possible file to play
    if (new_index != 0 && file != *curr_playing_ && !std::filesystem::is_directory(file)) {
      found = true;
      break;
    }

    new_index = is_next ? (new_index + 1) % size : (new_index + size - 1) % size;
  }

  return found ? file : File{};
}

}  // namespace interface
