#include "view/block/sidebar_content/playlist_manager.h"

#include <algorithm>
#include <cmath>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <thread>

#include "audio/lyric/lyric_finder.h"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/keybinding.h"
#include "view/element/util.h"

namespace interface {

PlaylistManager::PlaylistManager(const model::BlockIdentifier& id,
                                 const std::shared_ptr<EventDispatcher>& dispatcher,
                                 const FocusCallback& on_focus, const keybinding::Key& keybinding,
                                 const std::shared_ptr<util::FileHandler>& file_handler,
                                 int max_columns)
    : TabItem(id, dispatcher, on_focus, keybinding, std::string{kTabName}),
      file_handler_(file_handler),
      max_columns_(max_columns) {
  model::Playlists parsed;

  // Parse cache to check if exists any playlist saved from before
  if (file_handler_->ParsePlaylists(parsed) && !parsed.empty()) {
    entries_.reserve(parsed.size());

    for (const auto& playlist : parsed) {
      entries_.push_back(InternalPlaylist{
          .playlist = playlist,
          .collapsed = false,
      });
    }
  }
}

/* ********************************************************************************************** */

ftxui::Element PlaylistManager::Render() {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  auto max_size = ftxui::size(WIDTH, EQUAL, max_columns_);

  Clamp();
  ftxui::Elements entries;
  entries.reserve(Size());

  const int* selected = GetSelected();
  const int* focused = GetFocused();
  int index = 0;

  // Get all playlists
  for (const auto& entry : entries_) {
    bool is_curr_playing = curr_playing_ && entry.playlist.name == curr_playing_->playlist;
    bool is_focused = (*focused == index);
    bool is_selected = (*selected == index);

    auto prefix = ftxui::text(entry.collapsed ? "▼ " : "▶ ");
    auto text = ftxui::text(entry.playlist.name);

    ftxui::Decorator playlist_style =
        is_selected ? (is_focused ? styles_.playlist.selected_focused : styles_.playlist.selected)
                    : (is_focused ? styles_.playlist.focused : styles_.playlist.normal);

    auto focus_management = is_focused ? ftxui::select : ftxui::nothing;

    entries.push_back(ftxui::hbox({
                          prefix | styles_.prefix,
                          text | playlist_style | ftxui::xflex,
                      }) |
                      max_size | focus_management | ftxui::reflect(boxes_[index]));

    if (!entry.collapsed) {
      index++;
      continue;
    }

    // Get all songs from that playlist
    for (const auto& song : entry.playlist.songs) {
      index++;
      is_focused = (*focused == index);
      is_selected = (*selected == index);

      prefix = ftxui::text("  ");
      text = ftxui::text(" " + song.filepath.filename().string());

      const auto& type = is_curr_playing && curr_playing_->filepath == song.filepath
                             ? styles_.playing
                             : styles_.song;

      ftxui::Decorator song_style = is_selected
                                        ? (is_focused ? type.selected_focused : type.selected)
                                        : (is_focused ? type.focused : type.normal);

      focus_management = is_focused ? ftxui::select : ftxui::nothing;

      entries.push_back(ftxui::hbox({
                            prefix | styles_.prefix,
                            text | song_style | ftxui::xflex,
                        }) |
                        max_size | focus_management | ftxui::reflect(boxes_[index]));
    }

    index++;
  }

  return ftxui::vbox(entries) | ftxui::reflect(box_) | ftxui::frame | ftxui::flex;
}

/* ********************************************************************************************** */

bool PlaylistManager::OnEvent(const ftxui::Event& event) {
  if (Size() && OnMenuNavigation(event)) return true;

  return false;
}

/* ********************************************************************************************** */

bool PlaylistManager::OnMouseEvent(ftxui::Event& event) {
  if (event.mouse().button == ftxui::Mouse::WheelDown ||
      event.mouse().button == ftxui::Mouse::WheelUp)
    return OnMouseWheel(event);

  if (event.mouse().button != ftxui::Mouse::Left && event.mouse().button != ftxui::Mouse::None)
    return false;

  int* selected = GetSelected();
  int* focused = GetFocused();

  for (int i = 0; i < Size(); ++i) {
    if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) continue;

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

  return false;
}

/* ********************************************************************************************** */

bool PlaylistManager::OnCustomEvent(const CustomEvent& event) {
  if (event == CustomEvent::Identifier::UpdateSongInfo) {
    LOG("Received new playlist song information from player");

    // Set current song
    curr_playing_ = event.GetContent<model::Song>();
  }

  return false;
}

/* ********************************************************************************************** */

bool PlaylistManager::OnMouseWheel(ftxui::Event event) {
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

bool PlaylistManager::OnMenuNavigation(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  bool event_handled = false;
  int* selected = GetSelected();
  int* focused = GetFocused();

  int old_selected = *selected;

  if (event == Keybind::ArrowUp || event == Keybind::Up) *selected = *selected - 1;
  if (event == Keybind::ArrowDown || event == Keybind::Down) *selected = *selected + 1;
  if (event == Keybind::PageUp) (*selected) -= box_.y_max - box_.y_min;
  if (event == Keybind::PageDown) (*selected) += box_.y_max - box_.y_min;
  if (event == Keybind::Home) (*selected) = 0;
  if (event == Keybind::End) (*selected) = Size() - 1;

  if (event == Keybind::ArrowLeft || event == Keybind::Left) {
    int index = 0;

    for (auto& entry : entries_) {
      if (index == *selected) {
        LOG("Handle menu navigation key=", util::EventToString(event));
        Clamp();
        entry.collapsed = false;
        event_handled = true;
        break;
      }

      index += entry.playlist.songs.size();
    }
  }

  if (event == Keybind::ArrowRight || event == Keybind::Right) {
    int index = 0;

    for (auto& entry : entries_) {
      if (index == *selected) {
        LOG("Handle menu navigation key=", util::EventToString(event));
        Clamp();
        entry.collapsed = true;
        event_handled = true;
        break;
      }

      index += entry.playlist.songs.size();
    }
  }

  if (*selected != old_selected) {
    *selected = clamp(*selected, 0, Size() - 1);

    if (*selected != old_selected) {
      LOG("Handle menu navigation key=", util::EventToString(event));
      *focused = *selected;
      event_handled = true;
    }

    // TODO: add support for TextAnimation element
  }

  // Otherwise, user may want to change current directory
  if (event == Keybind::Return) {
    LOG("Handle menu navigation key=", util::EventToString(event));
    event_handled = ClickOnActiveEntry();
  }

  return event_handled;
}

/* ********************************************************************************************** */

void PlaylistManager::Clamp() {
  boxes_.resize(Size());

  int* selected = GetSelected();
  int* focused = GetFocused();

  *selected = clamp(*selected, 0, Size() - 1);
  *focused = clamp(*focused, 0, Size() - 1);
}

/* ********************************************************************************************** */

bool PlaylistManager::ClickOnActiveEntry() {
  if (entries_.empty()) return false;

  int* selected = GetSelected();

  int index = 0;
  for (const auto& entry : entries_) {
    if (index == *selected) {
      // Selected index is on playlist title
      auto dispatcher = dispatcher_.lock();
      if (!dispatcher) return false;

      auto event_selection = interface::CustomEvent::NotifyPlaylistSelection(entry.playlist);
      dispatcher->SendEvent(event_selection);

      return true;
    }

    if (entry.collapsed) {
      for (auto it = entry.playlist.songs.begin(); it != entry.playlist.songs.end(); ++it) {
        index++;

        if (index == *selected) {
          // Selected index is on song from playlist
          auto dispatcher = dispatcher_.lock();
          if (!dispatcher) return false;

          // Shuffle playlist based on selected entry
          model::Playlist playlist{
              .name = entry.playlist.name,
              .songs = std::deque<model::Song>(it, entry.playlist.songs.end()),
          };
          playlist.songs.insert(playlist.songs.end(), entry.playlist.songs.begin(), it);

          auto event_selection = interface::CustomEvent::NotifyPlaylistSelection(playlist);
          dispatcher->SendEvent(event_selection);

          return true;
        }
      }
    }

    index++;
  }

  return false;
}

}  // namespace interface
