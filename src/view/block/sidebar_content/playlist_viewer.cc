#include "view/block/sidebar_content/playlist_viewer.h"

#include <functional>
#include <iomanip>
#include <string>

#include "ftxui/dom/elements.hpp"
#include "model/question_data.h"
#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {

Button::Style PlaylistViewer::kButtonStyle = Button::Style{
    .normal =
        Button::Style::State{
            .foreground = ftxui::Color::Grey11,
            .background = ftxui::Color::SteelBlue1,
        },
    .focused =
        Button::Style::State{
            .foreground = ftxui::Color::DeepSkyBlue4Ter,
            .background = ftxui::Color::LightSkyBlue1,
        },
    .pressed =
        Button::Style::State{
            .foreground = ftxui::Color::SkyBlue1,
            .background = ftxui::Color::Blue1,
        },
    .disabled =
        Button::Style::State{
            .foreground = ftxui::Color::Grey35,
            .background = ftxui::Color::SteelBlue,
        },
};

/* ********************************************************************************************** */

PlaylistViewer::PlaylistViewer(const model::BlockIdentifier& id,
                               const std::shared_ptr<EventDispatcher>& dispatcher,
                               const FocusCallback& on_focus, const keybinding::Key& keybinding,
                               const std::shared_ptr<util::FileHandler>& file_handler,
                               int max_columns)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string{kTabName}),
      max_columns_(max_columns),
      file_handler_(file_handler),
      menu_(menu::CreatePlaylistMenu(
          dispatcher,

          // Callback to force a UI refresh
          [this] {
            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return;

            dispatcher->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this](const auto& active) {
            if (!active || active->songs.empty()) return false;

            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return false;

            LOG("Handle on_click event on playlist menu entry=", std::quoted(active->name));
            auto event_selection = interface::CustomEvent::NotifyPlaylistSelection(*active);
            dispatcher->SendEvent(event_selection);

            return true;
          })) {
  // Set max columns for an entry in menu
  menu_->SetMaxColumns(max_columns);

  // Initialize playlist buttons
  CreateButtons();

  // Attempt to parse playlists file
  if (model::Playlists parsed; file_handler_->ParsePlaylists(parsed) && !parsed.empty()) {
    menu_->SetEntries(parsed);

    // Enable them again
    btn_modify_->Enable();
    btn_delete_->Enable();
  }
}

/* ********************************************************************************************** */

ftxui::Element PlaylistViewer::Render() {
  static constexpr int kNumberOfElements = 2;

  ftxui::Elements entries;
  entries.reserve(kNumberOfElements);

  entries.push_back(menu_->Render());

  // Append all buttons at the bottom of the block
  entries.push_back(ftxui::hbox({
      ftxui::filler(),
      btn_create_->Render() | ftxui::bold,
      ftxui::filler(),
      btn_modify_->Render() | ftxui::bold,
      ftxui::filler(),
      btn_delete_->Render() | ftxui::bold,
      ftxui::filler(),
  }));

  return ftxui::vbox(entries) | ftxui::reflect(box_) | ftxui::frame | ftxui::flex;
}

/* ********************************************************************************************** */

bool PlaylistViewer::OnEvent(const ftxui::Event& event) {
  if (menu_->OnEvent(event)) return true;

  if (event == keybinding::Playlist::Create) {
    LOG("Handle key to invoke playlist dialog for create");
    btn_create_->OnClick();
    return true;
  }

  if (event == keybinding::Playlist::Modify && btn_modify_->IsActive()) {
    LOG("Handle key to invoke playlist dialog for modify");
    btn_modify_->OnClick();
    return true;
  }

  if (event == keybinding::Playlist::Delete && btn_delete_->IsActive()) {
    LOG("Handle key to invoke question dialog for delete");
    btn_delete_->OnClick();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool PlaylistViewer::OnMouseEvent(ftxui::Event& event) {
  if (menu_->OnMouseEvent(event)) return true;

  if (btn_create_->OnMouseEvent(event)) return true;
  if (btn_delete_->OnMouseEvent(event)) return true;
  if (btn_modify_->OnMouseEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

bool PlaylistViewer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new playlist song information from player");

    // Set current song
    auto current_song = event.GetContent<model::Song>();
    menu_->SetEntryHighlighted(current_song);
  }

  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    menu_->ResetHighlight();

    // TODO: force to clear highlight always, even when tab_item is not active
  }

  // Receive modified playlist from dialog
  if (event == CustomEvent::Identifier::SavePlaylistsToFile) {
    LOG("Save playlists to JSON");

    auto modified_playlist = event.GetContent<model::Playlist>();
    auto playlists_wrapper = menu_->GetEntries();

    model::Playlists playlists;
    playlists.reserve(playlists_wrapper.size());

    bool found = false;

    for (auto& playlist_wrapper : playlists_wrapper) {
      // If modified playlist is based on an existing one, just replace it
      if (playlist_wrapper.playlist.index == modified_playlist.index) {
        LOG("Changing playlist old=", playlist_wrapper.playlist, " to new=", modified_playlist);
        playlist_wrapper.playlist = modified_playlist;
        found = true;
      }

      playlists.emplace_back(playlist_wrapper.playlist);
    }

    // Otherwise, create a new entry for it
    if (!found) {
      LOG("Could not find a matching playlist, so create a new one");
      modified_playlist.index = playlists.size();
      playlists.emplace_back(modified_playlist);
    }

    bool result = file_handler_->SavePlaylists(playlists);
    LOG("Operation to save playlists in a JSON file, result=", result ? "success" : "error");

    // TODO: think if should create new method to edit existing entry
    // Update UI state
    menu_->SetEntries(playlists);

    // Make sure to enable them
    btn_modify_->Enable();
    btn_delete_->Enable();

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void PlaylistViewer::OnFocus() {
  // Attempt to parse playlists file
  if (model::Playlists parsed; file_handler_->ParsePlaylists(parsed) && !parsed.empty()) {
    menu_->SetEntries(parsed);

    // Enable them again
    btn_modify_->Enable();
    btn_delete_->Enable();
  } else {
    // Make sure to disable them
    btn_modify_->Disable();
    btn_delete_->Disable();
  }
}

/* ********************************************************************************************** */

void PlaylistViewer::CreateButtons() {
  btn_create_ = Button::make_button_minimal(
      std::string("create"),
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Create button");

        // Set this block as active (focused)
        on_focus_();

        model::PlaylistOperation operation{
            .action = model::PlaylistOperation::Operation::Create,
            .playlist = model::Playlist{},
        };

        auto event = interface::CustomEvent::ShowPlaylistManager(operation);
        disp->SendEvent(event);

        return true;
      },
      kButtonStyle);

  btn_modify_ = Button::make_button_minimal(
      std::string("modify"),
      [this]() {
        auto dispatcher = dispatcher_.lock();
        const auto& entry = menu_->GetActiveEntry();

        if (!dispatcher || !entry) return false;

        LOG("Handle callback for Modify button, entry=", *entry);

        // Set this block as active (focused)
        on_focus_();

        model::PlaylistOperation operation{
            .action = model::PlaylistOperation::Operation::Modify,
            .playlist = *entry,
        };

        auto event = interface::CustomEvent::ShowPlaylistManager(operation);
        dispatcher->SendEvent(event);

        return true;
      },
      kButtonStyle);

  btn_delete_ = Button::make_button_minimal(
      std::string("delete"),
      [this]() {
        auto dispatcher = dispatcher_.lock();
        const auto& entry = menu_->GetActiveEntry();

        if (!dispatcher || !entry) return false;

        LOG("Handle callback for Delete button, entry=", *entry);

        // Set this block as active (focused)
        on_focus_();

        model::QuestionData content{
            .question = std::string("Do you want to delete \"" + entry->name + "\"?"),
            .cb_yes = std::bind(&PlaylistViewer::OnYes, this),
        };

        auto event_dialog = interface::CustomEvent::ShowQuestionDialog(content);
        dispatcher->SendEvent(event_dialog);

        return true;
      },
      kButtonStyle);

  // Start with them disabled, until we parse some playlist from cache file
  btn_modify_->Disable();
  btn_delete_->Disable();
}

/* ********************************************************************************************** */

void PlaylistViewer::OnYes() {
  // Receive delete operation from question dialog
  const auto& entry = menu_->GetActiveEntry();
  if (!entry) return;

  LOG("Deleting playlist=", *entry);
  menu_->Erase(*entry);

  const auto& playlists_wrapper = menu_->GetEntries();

  model::Playlists playlists;
  playlists.reserve(playlists_wrapper.size());

  for (const auto& playlist_wrapper : playlists_wrapper) {
    playlists.emplace_back(playlist_wrapper.playlist);
  }

  bool result = file_handler_->SavePlaylists(playlists);
  LOG("Operation to save playlists in JSON file, result=", result ? "success" : "error");

  if (playlists.empty()) {
    // Make sure to disable them
    btn_modify_->Disable();
    btn_delete_->Disable();
  }
}

}  // namespace interface
