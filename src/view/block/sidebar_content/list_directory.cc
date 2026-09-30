#include "view/block/sidebar_content/list_directory.h"

#include <algorithm>
#include <filesystem>
#include <memory>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/application_error.h"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"
#include "view/base/keybinding.h"

namespace interface {

ListDirectory::ListDirectory(const model::BlockIdentifier& id,
                             const std::shared_ptr<EventDispatcher>& dispatcher,
                             const FocusCallback& on_focus, const keybinding::Key& keybinding,
                             const std::shared_ptr<util::FileHandler>& file_handler,
                             int max_columns, const std::string& optional_path,
                             const AudioCheckCallback& contains_audio_cb)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string(kTabName)),
      max_columns_(max_columns),
      contains_audio_cb_(contains_audio_cb),
      menu_(menu::CreateFileMenu(
          dispatcher, file_handler,

          // Callback to force a UI refresh
          [this] {
            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return;

            dispatcher->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this](const std::optional<util::File>& active) {
            if (!active) return false;

            // Send user action to controller, try to play selected entry
            LOG("Handle on_click event on menu entry=", *active);
            return SendFileSelection(*active);
          },
          menu::Style::Default, optional_path)) {
  // Set max columns for an entry in menu
  menu_->SetMaxColumns(max_columns);
}

/* ********************************************************************************************** */

ftxui::Element ListDirectory::Render() {
  // Build up the whole content
  return menu_->Render();
}

/* ********************************************************************************************** */

bool ListDirectory::OnEvent(const ftxui::Event& event) {
  if (menu_->OnEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

void ListDirectory::OnFocus() {
  // Files may have changed while this list was not visible/focused
  menu_->actual().Reload();
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
    auto active = menu_->GetActiveEntry();
    if (!active) return false;

    SendFileSelection(*active);
    return true;
  }

  if (event == CustomEvent::Identifier::SkipToPreviousSong) {
    LOG("Received request from media player to play previous song");

    if (auto file = SelectFileToPlay(/*pick_next=*/false); !file.empty()) {
      LOG("Skipping song, attempt to play previous file: ", file);
      // Send user action to controller
      SendFileSelection(file);
    }

    return true;
  }

  if (event == CustomEvent::Identifier::SkipToNextSong) {
    LOG("Received request from media player to play next song");

    if (auto file = SelectFileToPlay(/*pick_next=*/true); !file.empty()) {
      LOG("Skipping song, attempt to play next file: ", file);
      // Send user action to controller
      SendFileSelection(file);
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
        SendFileSelection(file);
      }
    }
  }
#endif

  return false;
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
    // Found a possible file to play
    if (new_index != 0 && file != *curr_playing_ && !std::filesystem::is_directory(file) &&
        (!contains_audio_cb_ || contains_audio_cb_(file))) {
      found = true;
      break;
    }

    new_index = is_next ? (new_index + 1) % size : (new_index + size - 1) % size;
  }

  return found ? file : util::File{};
}

/* ********************************************************************************************** */

bool ListDirectory::SendFileSelection(const util::File& file) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return false;

  // Do not send it to audio thread, otherwise current song would be stopped for nothing
  if (contains_audio_cb_ && !contains_audio_cb_(file)) {
    ERROR("Selected file does not contain an audio stream, file=", file);
    dispatcher->SetApplicationError(error::kFileNotSupported, file.filename().string());
    return true;
  }

  auto event_selection = interface::CustomEvent::NotifyFileSelection(file);
  dispatcher->SendEvent(event_selection);

  return true;
}

}  // namespace interface
