#include "view/block/sidebar_content/playlist_viewer.h"

#include <iomanip>
#include <string>

#include "ftxui/dom/elements.hpp"
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
            auto disp = dispatcher_.lock();
            if (!disp) return;

            disp->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this](const auto& active) {
            if (!active) return false;

            if (active->songs.empty()) return false;

            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return false;

            LOG("Handle on_click event on playlist menu entry=", std::quoted(active->name));
            auto event_selection = interface::CustomEvent::NotifyPlaylistSelection(*active);
            dispatcher->SendEvent(event_selection);

            return true;
          })) {
  // Set max columns for an entry in menu
  menu_->SetMaxColumns(max_columns);

  // Attempt to parse playlists file
  if (model::Playlists parsed; file_handler_->ParsePlaylists(parsed) && !parsed.empty()) {
    menu_->SetEntries(parsed);
  }

  // Initialize playlist buttons
  CreateButtons();
}

/* ********************************************************************************************** */

ftxui::Element PlaylistViewer::Render() {
  ftxui::Elements entries;
  entries.reserve(2);

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

  if (event == keybinding::Playlist::Create || event == keybinding::Playlist::Modify ||
      event == keybinding::Playlist::Delete) {
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return false;

    model::PlaylistOperation operation{
        .action =
            event == keybinding::Playlist::Create   ? model::PlaylistOperation::Operation::Create
            : event == keybinding::Playlist::Modify ? model::PlaylistOperation::Operation::Modify
                                                    : model::PlaylistOperation::Operation::Delete,
        .playlist = menu_->GetActiveEntry(),
    };

    LOG("Handle key to open playlist dialog, operation=", operation);
    auto event_dialog = interface::CustomEvent::ShowPlaylistManager(operation);
    dispatcher->SendEvent(event_dialog);

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

  if (event == CustomEvent::Identifier::SavePlaylistsToFile) {
    LOG("Save playlists to JSON");

    auto modified_playlist = event.GetContent<model::Playlist>();
    auto playlists_wrapper = menu_->GetEntries();

    model::Playlists playlists;
    playlists.reserve(playlists_wrapper.size());

    bool found = false;

    for (auto& playlist_wrapper : playlists_wrapper) {
      if (playlist_wrapper.playlist.index == modified_playlist.index) {
        LOG("Changing playlist old=", playlist_wrapper.playlist, " to new=", modified_playlist);
        playlist_wrapper.playlist = modified_playlist;
        found = true;
      }

      playlists.emplace_back(playlist_wrapper.playlist);
    }

    if (!found) {
      LOG("Could not find a matching playlist, so create a new one");
      modified_playlist.index = playlists.size();
      playlists.emplace_back(modified_playlist);
    }

    // TODO: think if should create new method to edit existing entry
    menu_->SetEntries(playlists);

    bool result = file_handler_->SavePlaylists(playlists);
    LOG("Operation to save playlists in a JSON file, result=", result ? "success" : "error");

    return true;
  }

  return false;
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
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Modify button");

        // Set this block as active (focused)
        on_focus_();

        model::PlaylistOperation operation{
            .action = model::PlaylistOperation::Operation::Modify,
            .playlist = menu_->GetActiveEntry(),
        };

        auto event = interface::CustomEvent::ShowPlaylistManager(operation);
        disp->SendEvent(event);

        return true;
      },
      kButtonStyle);

  btn_delete_ = Button::make_button_minimal(
      std::string("delete"),
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Delete button");

        // Set this block as active (focused)
        on_focus_();

        model::PlaylistOperation operation{
            .action = model::PlaylistOperation::Operation::Delete,
            .playlist = menu_->GetActiveEntry(),
        };

        auto event = interface::CustomEvent::ShowPlaylistManager(operation);
        disp->SendEvent(event);

        return true;
      },
      kButtonStyle);
}

}  // namespace interface
