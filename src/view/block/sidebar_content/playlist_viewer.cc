#include "view/block/sidebar_content/playlist_viewer.h"

#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>

#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {

Button::Style PlaylistViewer::kTabButtonStyle = Button::Style{
    .normal =
        Button::Style::State{
            .foreground = ftxui::Color::DarkBlue,
            .background = ftxui::Color::SteelBlue1,
        },
    .focused =
        Button::Style::State{
            .foreground = ftxui::Color::GrayLight,
            .background = ftxui::Color::GrayDark,
        },
    .selected =
        Button::Style::State{
            .foreground = ftxui::Color::DarkBlue,
            .background = ftxui::Color::DodgerBlue1,
        },

    .delimiters = Button::Delimiters{" ", " "},
};

/* ********************************************************************************************** */

PlaylistViewer::PlaylistViewer(const model::BlockIdentifier& id,
                               const std::shared_ptr<EventDispatcher>& dispatcher,
                               const FocusCallback& on_focus, const keybinding::Key& keybinding,
                               const std::shared_ptr<util::FileHandler>& file_handler,
                               int max_columns)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string{kTabName}),
      file_handler_(file_handler),
      max_columns_(max_columns) {
  // TODO: Think how to disable this on unit testing
  // Attempt to parse playlists file
  // if (model::Playlists parsed; file_handler_->ParsePlaylists(parsed) && !parsed.empty()) {
  //   entries_.reserve(parsed.size());
  //
  //   for (const auto& playlist : parsed) {
  //     entries_.push_back(InternalPlaylist{
  //         .playlist = playlist,
  //         .collapsed = false,
  //     });
  //   }
  // }

  // Initialize playlist buttons
  CreateButtons();
}

/* ********************************************************************************************** */

ftxui::Element PlaylistViewer::Render() {
  ftxui::Elements entries;

  // Append all buttons at the bottom of the block
  entries.push_back(ftxui::filler());
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
  // TODO: implement
  return false;
}

/* ********************************************************************************************** */

bool PlaylistViewer::OnMouseEvent(ftxui::Event& event) {
  if (btn_create_->OnMouseEvent(event)) return true;
  if (btn_delete_->OnMouseEvent(event)) return true;
  if (btn_modify_->OnMouseEvent(event)) return true;

  // TODO: implement
  return false;
}

/* ********************************************************************************************** */

bool PlaylistViewer::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new playlist song information from player");

    // Set current song
    curr_playing_ = event.GetContent<model::Song>();
  }

  if (event == CustomEvent::Identifier::ClearSongInfo) {
    LOG("Clear current song information");
    curr_playing_.reset();
  }

  return false;
}

/* ********************************************************************************************** */

// bool PlaylistViewer::ClickOnActiveEntry() {
//   if (entries_.empty()) return false;
//
//   int* selected = GetSelected();
//
//   int index = 0;
//   for (const auto& entry : entries_) {
//     if (index == *selected) {
//       // Selected index is on playlist title
//       auto dispatcher = dispatcher_.lock();
//       if (!dispatcher) return false;
//
//       auto event_selection = interface::CustomEvent::NotifyPlaylistSelection(entry.playlist);
//       dispatcher->SendEvent(event_selection);
//
//       return true;
//     }
//
//     if (entry.collapsed) {
//       for (auto it = entry.playlist.songs.begin(); it != entry.playlist.songs.end();
//            ++it, ++index) {
//         if (index == *selected) {
//           // Selected index is on song from playlist
//           auto dispatcher = dispatcher_.lock();
//           if (!dispatcher) return false;
//
//           // Shuffle playlist based on selected entry
//           model::Playlist playlist{
//               .name = entry.playlist.name,
//               .songs = std::deque<model::Song>(it, entry.playlist.songs.end()),
//           };
//           playlist.songs.insert(playlist.songs.end(), entry.playlist.songs.begin(), it);
//
//           auto event_selection = interface::CustomEvent::NotifyPlaylistSelection(playlist);
//           dispatcher->SendEvent(event_selection);
//
//           return true;
//         }
//       }
//     }
//
//     index++;
//   }
//
//   return false;
// }

/* ********************************************************************************************** */

void PlaylistViewer::CreateButtons() {
  btn_create_ = Button::make_button_for_window(
      std::string("create"),
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Create button");

        // TODO: call new dialog to create and manage playlist entries

        // Set this block as active (focused)
        on_focus_();

        return true;
      },
      kTabButtonStyle);

  btn_modify_ = Button::make_button_for_window(
      std::string("modify"),
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Create button");

        // TODO: call new dialog to manage playlist entries

        // Set this block as active (focused)
        on_focus_();

        return true;
      },
      kTabButtonStyle);

  btn_delete_ = Button::make_button_for_window(
      std::string("delete"),
      [this]() {
        auto disp = dispatcher_.lock();
        if (!disp) return false;

        LOG("Handle callback for Create button");

        // TODO: call new dialog to delete playlist

        // Set this block as active (focused)
        on_focus_();

        return true;
      },
      kTabButtonStyle);
}

}  // namespace interface
