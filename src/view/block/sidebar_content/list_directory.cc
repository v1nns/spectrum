#include "view/block/sidebar_content/list_directory.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <system_error>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/application_error.h"
#include "model/playlist.h"
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
  if (menu_->OnMouseEvent(event)) {
    // Set focus on parent block, so keys go to this list after clicking on it
    if (on_focus_) on_focus_();

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool ListDirectory::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new song information from player");

    // Set current song
    const auto& song = event.GetContent<model::Song>();
    curr_playing_ = song.filepath;

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

  return false;
}

/* ********************************************************************************************** */

model::Playlist ListDirectory::CreateQueue(const util::File& file) {
  model::Playlist queue{.index = -1, .songs = {model::Song{.filepath = file}}};

  // Starting from the selected file, every other media file from the list is played once (and in
  // case of error, player simply skips to the next one)
  const auto entries = menu_->GetEntries();
  auto selected = std::find(entries.begin(), entries.end(), file);
  if (selected == entries.end()) return queue;

  auto add_to_queue = [&queue](const util::File& entry) {
    std::error_code error;
    if (!std::filesystem::is_directory(entry, error) &&
        internal::FileMenu::HasMediaExtension(entry)) {
      queue.songs.push_back(model::Song{.filepath = entry});
    }
  };

  std::for_each(std::next(selected), entries.end(), add_to_queue);
  std::for_each(entries.begin(), selected, add_to_queue);

  return queue;
}

/* ********************************************************************************************** */

bool ListDirectory::SendFileSelection(const util::File& file) {
  auto dispatcher = dispatcher_.lock();
  if (!dispatcher) return false;

  // Do not send it to audio thread, otherwise current song would be stopped for nothing
  if (contains_audio_cb_ && !contains_audio_cb_(file)) {
    WARN("Selected file does not contain an audio stream, file=", file);
    dispatcher->SetApplicationError(error::kFileNotSupported, file.filename().string());
    return true;
  }

  // Send directory files as a queue, so player can play next/previous file by itself
  auto event_selection = interface::CustomEvent::NotifyPlaylistSelection(CreateQueue(file));
  dispatcher->SendEvent(event_selection);

  return true;
}

}  // namespace interface
