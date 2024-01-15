#include "view/block/sidebar_content/list_directory.h"

#include <algorithm>
#include <filesystem>
#include <ftxui/dom/elements.hpp>
#include <iomanip>
#include <memory>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/screen/color.hpp"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"
#include "view/base/keybinding.h"

namespace interface {

ListDirectory::ListDirectory(const model::BlockIdentifier& id,
                             const std::shared_ptr<EventDispatcher>& dispatcher,
                             const FocusCallback& on_focus, const keybinding::Key& keybinding,
                             const std::shared_ptr<util::FileHandler>& file_handler,
                             int max_columns, const std::string& optional_path)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string(kTabName)),
      file_handler_(file_handler),
      max_columns_(max_columns),
      menu_(menu::CreateFileMenu(
          dispatcher,

          // Callback to force a UI refresh
          [this] {
            auto disp = dispatcher_.lock();
            if (!disp) return;

            disp->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this](const std::optional<util::File>& active) {
            if (!active) return false;

            LOG("Handle on_click event on menu entry=", *active);
            std::filesystem::path new_dir;

            if (active->filename() == ".." && std::filesystem::exists(curr_dir_.parent_path())) {
              // Change to parent folder
              new_dir = curr_dir_.parent_path();
            } else if (std::filesystem::is_directory(*active)) {
              // Change to selected folder
              new_dir = curr_dir_ / active->filename();
            } else {
              // Send user action to controller, try to play selected entry
              auto dispatcher = dispatcher_.lock();
              if (!dispatcher) return false;

              auto event_selection = interface::CustomEvent::NotifyFileSelection(*active);
              dispatcher->SendEvent(event_selection);

              return true;
            }

            if (!new_dir.empty()) {
              RefreshList(new_dir);
              return true;
            }

            return false;
          })) {
  // Set max columns for an entry in menu
  menu_->SetMaxColumns(max_columns);

  // TODO: this is not good, read this below
  // https://google.github.io/styleguide/cppguide.html#Doing_Work_in_Constructors
  auto filepath = ComposeDirectoryPath(optional_path);

  if (bool parsed = RefreshList(filepath); !optional_path.empty() && !parsed) {
    // If we can't list files from current path, then everything is gone
    RefreshList(std::filesystem::current_path());
  }
}

/* ********************************************************************************************** */

ftxui::Element ListDirectory::Render() {
  // Build up the whole content
  return ftxui::vbox({
      ftxui::text(GetTitle()) | ftxui::color(ftxui::Color::White) | ftxui::bold,
      menu_->Render(),
  });
}

/* ********************************************************************************************** */

bool ListDirectory::OnEvent(const ftxui::Event& event) {
  if (menu_->OnEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnMouseEvent(ftxui::Event& event) {
  if (menu_->OnMouseEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");

    // Set current song
    curr_playing_ = event.GetContent<model::Song>().filepath;

    // Update highlighted entry in menu
    menu_->ResetSearch();
    menu_->SetEntryHighlighted(*curr_playing_);
  }

  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    curr_playing_.reset();
    menu_->ResetHighlight();
  }

  if (event == CustomEvent::Identifier::PlaySong) {
    LOG("Received request from media player to play selected file");
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    auto active = menu_->GetActiveEntry();
    if (!active) return false;

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

std::string ListDirectory::GetTitle() {
  const std::string curr_dir = curr_dir_.string();

  // Everything fine, directory does not exceed maximum column length
  if (curr_dir.size() <= max_columns_) {
    return curr_dir;
  }

  // Oh no, it does exceed, so we must truncate the exceeding text
  int offset =
      (int)curr_dir.size() - (max_columns_ - 5);  // Considering window border(2) + ellipsis(3)
  const std::string& substr = curr_dir.substr(offset);
  auto index = substr.find('/');

  return index != std::string::npos ? std::string("..." + substr.substr(index)) : substr;
}

/* ********************************************************************************************** */

std::filesystem::path ListDirectory::ComposeDirectoryPath(const std::string& optional_path) {
  // By default, use current path from where spectrum was executed
  std::filesystem::path filepath = std::filesystem::current_path();

  if (optional_path.empty()) return filepath;

  // Remove last slash from given path
  std::string clean_path = optional_path.back() == '/'
                               ? optional_path.substr(0, optional_path.size() - 1)
                               : optional_path;

  // Check if given path is valid
  try {
    auto tmp = std::filesystem::canonical(clean_path);
    filepath = tmp;
  } catch (...) {
    ERROR("Invalid path, tried to compose canonical path using ", std::quoted(clean_path));
  }

  return filepath;
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

  LOG("Updating list with new entries, size=", tmp.size());

  // Reset internal values
  curr_dir_ = dir_path;
  menu_->SetEntries(tmp);

  return true;
}

/* ********************************************************************************************** */

util::File ListDirectory::SelectFileToPlay(bool is_next) {
  // Get entries from menu element
  const auto& entries = menu_->GetEntries();
  int size = static_cast<int>(entries.size());

  if (size <= 2) return util::File{};

  // Get index from current song playing
  int index = static_cast<int>(
      std::distance(entries.begin(), std::find(entries.begin(), entries.end(), *curr_playing_)));

  int new_index = is_next ? (index + 1) % size : (index + size - 1) % size;
  int attempts = size;
  util::File file;

  // Iterate circularly through all file entries
  bool found = false;
  for (file = entries[new_index]; attempts > 0; --attempts, file = entries[new_index]) {
    // TODO: create API on decoder to check if this file contains an audio stream

    // Found a possible file to play
    if (new_index != 0 && file != *curr_playing_ && !std::filesystem::is_directory(file)) {
      found = true;
      break;
    }

    new_index = is_next ? (new_index + 1) % size : (new_index + size - 1) % size;
  }

  return found ? file : util::File{};
}

}  // namespace interface
