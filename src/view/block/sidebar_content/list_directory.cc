#include "view/block/sidebar_content/list_directory.h"

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
#include "view/base/keybinding.h"
#include "view/element/util.h"

namespace interface {

ListDirectory::ListDirectory(const model::BlockIdentifier& id,
                             const std::shared_ptr<EventDispatcher>& dispatcher,
                             const FocusCallback& on_focus, const keybinding::Key& keybinding,
                             const std::shared_ptr<util::FileHandler>& file_handler,
                             int max_columns, const std::string& optional_path)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string(kTabName)),
      file_handler_(file_handler),
      max_columns_(max_columns) {
  // Remove last slash
  std::string clean_path = optional_path.back() == '/'
                               ? optional_path.substr(0, optional_path.size() - 1)
                               : optional_path;

  // TODO: this is not good, read this below
  // https://google.github.io/styleguide/cppguide.html#Doing_Work_in_Constructors
  auto path =
      !clean_path.empty() ? std::filesystem::path(clean_path) : std::filesystem::current_path();

  if (bool parsed = RefreshList(path); !optional_path.empty() && !parsed) {
    // If we can't list files from current path, then everything is gone
    RefreshList(std::filesystem::current_path());
  }

  animation_.cb_update = [this] {
    // Send user action to controller
    auto disp = dispatcher_.lock();
    if (!disp) return;

    disp->SendEvent(interface::CustomEvent::Refresh());
  };
}

/* ********************************************************************************************** */

ftxui::Element ListDirectory::Render() {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  auto max_size = ftxui::size(WIDTH, EQUAL, max_columns_);

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

    const util::File& entry = GetEntry(i);
    const auto& type = entry == curr_playing_                 ? styles_.playing
                       : std::filesystem::is_directory(entry) ? styles_.directory
                                                              : styles_.file;
    auto prefix = ftxui::text(is_selected ? "▶ " : "  ");

    ftxui::Decorator style = is_selected ? (is_focused ? type.selected_focused : type.selected)
                                         : (is_focused ? type.focused : type.normal);

    auto focus_management = is_focused ? ftxui::select : ftxui::nothing;

    // In case of entry text too long, animation thread will be running, so we gotta take the text
    // content from there
    auto text = ftxui::text(animation_.enabled && is_selected ? animation_.text
                                                              : entry.filename().string());

    entries.push_back(ftxui::hbox({
                          prefix | styles_.prefix,
                          text | style | ftxui::xflex,
                      }) |
                      max_size | focus_management | ftxui::reflect(boxes_[i]));
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

  return ftxui::vbox(content);
}

/* ********************************************************************************************** */

bool ListDirectory::OnEvent(const ftxui::Event& event) {
  if (mode_search_ && OnSearchModeEvent(event)) {
    return true;
  }

  if (Size() && OnMenuNavigation(event)) {
    return true;
  }

  // Enable search mode
  if (!mode_search_ && event == keybinding::Files::EnableSearch) {
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

bool ListDirectory::OnMouseEvent(ftxui::Event& event) {
  if (event.mouse().button == ftxui::Mouse::WheelDown ||
      event.mouse().button == ftxui::Mouse::WheelUp)
    return OnMouseWheel(event);

  if (event.mouse().button != ftxui::Mouse::Left && event.mouse().button != ftxui::Mouse::None)
    return false;

  int* selected = GetSelected();
  int* focused = GetFocused();

  bool entry_focused = false;

  for (int i = 0; i < Size(); ++i) {
    if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) continue;

    entry_focused = true;
    *focused = i;

    if (event.mouse().button == ftxui::Mouse::Left &&
        event.mouse().motion == ftxui::Mouse::Released) {
      LOG("Handle left click mouse event on entry=", i);
      *selected = i;

      // Send event for setting focus on this block
      on_focus_();

      // Check if this is a double-click event
      auto now = std::chrono::system_clock::now();
      if (now - last_click_ <= std::chrono::milliseconds(500)) return ClickOnActiveEntry();

      last_click_ = now;
      return true;
    }
  }

  // If no entry was focused with mouse, reset index
  if (!entry_focused) *focused = *selected;

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
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

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
      auto dispatcher = dispatcher_.lock();
      if (!dispatcher) return false;

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
      auto dispatcher = dispatcher_.lock();
      if (!dispatcher) return false;

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
    // P.S.: in the future, remove this code block and make ListDirectory always send a queue of
    // files to AudioPlayer

    // In case that song has finished successfully, attempt to play next one
    if (auto content = event.GetContent<model::Song::CurrentInformation>();
        content.state == model::Song::MediaState::Finished) {
      if (auto file = SelectFileToPlay(/*pick_next=*/true); !file.empty()) {
        LOG("Song finished, attempt to play next file: ", file);
        // Send user action to controller
        auto dispatcher = dispatcher_.lock();
        if (!dispatcher) return false;

        auto event_selection = interface::CustomEvent::NotifyFileSelection(file);
        dispatcher->SendEvent(event_selection);
      }
    }
  }
#endif

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

  *selected = *focused;

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
  using Keybind = keybinding::Navigation;

  bool event_handled = false;
  int* selected = GetSelected();
  int* focused = GetFocused();

  int old_selected = *selected;

  if (event == Keybind::ArrowUp || event == Keybind::Up)
    *selected = (*selected + Size() - 1) % Size();
  if (event == Keybind::ArrowDown || event == Keybind::Down) *selected = (*selected + 1) % Size();
  if (event == Keybind::PageUp) (*selected) -= box_.y_max - box_.y_min;
  if (event == Keybind::PageDown) (*selected) += box_.y_max - box_.y_min;
  if (event == Keybind::Home) (*selected) = 0;
  if (event == Keybind::End) (*selected) = Size() - 1;

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
  if (event == Keybind::Return) {
    LOG("Handle menu navigation key=", util::EventToString(event));
    event_handled = ClickOnActiveEntry();
  }

  return event_handled;
}

/* ********************************************************************************************** */

bool ListDirectory::OnSearchModeEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  bool event_handled = false;
  bool exit_from_search_mode = false;

  // Any alphabetic character
  if (event.is_character()) {
    mode_search_->text_to_search.insert(mode_search_->position, event.character());
    mode_search_->position++;
    event_handled = true;
  }

  // Backspace
  if (event == Keybind::Backspace && !(mode_search_->text_to_search.empty())) {
    if (mode_search_->position > 0) {
      mode_search_->text_to_search.erase(mode_search_->position - 1, 1);
      mode_search_->position--;
    }
    event_handled = true;
  }

  // Ctrl + Backspace
  if (event == Keybind::CtrlBackspace || event == Keybind::CtrlBackspaceReverse) {
    mode_search_->text_to_search.clear();
    mode_search_->position = 0;
    event_handled = true;
  }

  // Arrow left
  if (event == Keybind::ArrowLeft) {
    if (mode_search_->position > 0) mode_search_->position--;
    event_handled = true;
  }

  // Arrow right
  if (event == Keybind::ArrowRight) {
    if (auto size = (int)mode_search_->text_to_search.size(); mode_search_->position < size)
      mode_search_->position++;
    event_handled = true;
  }

  // Quit search mode
  if (event == Keybind::Escape) {
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
  if (curr_dir.size() <= max_columns_) {
    return curr_dir;
  }

  // Oh no, it does exceed, so we must truncate the surplus text
  int offset =
      (int)curr_dir.size() - (max_columns_ - 5);  // Considering window border(2) + ellipsis(3)
  const std::string& substr = curr_dir.substr(offset);
  auto index = substr.find('/');

  return index != std::string::npos ? std::string("..." + substr.substr(index)) : substr;
}

/* ********************************************************************************************** */

bool ListDirectory::RefreshList(const std::filesystem::path& dir_path) {
  LOG("Refresh list with files from new directory=", std::quoted(dir_path.c_str()));
  util::Files tmp;

  if (!file_handler_->ListFiles(dir_path, tmp)) {
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    dispatcher->SetApplicationError(error::kAccessDirFailed);

    return false;
  }

  // Reset internal values
  curr_dir_ = dir_path;
  entries_.swap(tmp);
  selected_ = 0;
  focused_ = 0;

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
    std::string text{GetEntry(*selected).filename().string()};

    int max_chars = (int)text.length() + kMaxIconColumns;

    // Start animation thread
    if (max_chars > max_columns_) animation_.Start(text);
  }
}

/* ********************************************************************************************** */

util::File ListDirectory::SelectFileToPlay(bool is_next) {
  if (entries_.size() <= 2) return util::File{};

  // Get index from current song playing
  auto index = static_cast<int>(
      std::distance(entries_.begin(), std::find(entries_.begin(), entries_.end(), *curr_playing_)));

  auto size = static_cast<int>(entries_.size());

  int new_index = is_next ? (index + 1) % size : (index + size - 1) % size;
  int attempts = size;
  util::File file;

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

  return found ? file : util::File{};
}

/* ********************************************************************************************** */

bool ListDirectory::ClickOnActiveEntry() {
  std::filesystem::path new_dir;

  auto active = GetActiveEntry();
  if (active == nullptr) return false;

  if (active->filename() == ".." && std::filesystem::exists(curr_dir_.parent_path())) {
    new_dir = curr_dir_.parent_path();
  } else if (std::filesystem::is_directory(*active)) {
    new_dir = curr_dir_ / active->filename();
  } else {
    // Send user action to controller
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    auto event_selection = interface::CustomEvent::NotifyFileSelection(*active);
    dispatcher->SendEvent(event_selection);
    return true;
  }

  if (!new_dir.empty()) {
    RefreshList(new_dir);

    // Exit search mode if enabled
    mode_search_.reset();

    return true;
  }

  return false;
}

}  // namespace interface
