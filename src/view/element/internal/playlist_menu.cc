#include "view/element/internal/playlist_menu.h"

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/playlist.h"
#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {
namespace internal {

PlaylistMenu::PlaylistMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                           const TextAnimation::Callback& force_refresh, const Callback& on_click)
    : BaseMenu(dispatcher, force_refresh), on_click_{on_click} {}

/* ********************************************************************************************** */

model::Playlists PlaylistMenu::GetEntries() const {
  // Get list of entries without internal state
  auto dummy = IsSearchEnabled() ? *filtered_entries_ : entries_;

  model::Playlists playlists;
  playlists.reserve(dummy.size());

  for (const auto& entry : dummy) playlists.emplace_back(entry.playlist);
  return playlists;
}

/* ********************************************************************************************** */

ftxui::Element PlaylistMenu::RenderImpl() {
  ftxui::Elements menu_entries;
  menu_entries.reserve(GetSize() + 1);  // Entries size + filler element

  InternalPlaylists const* tmp = IsSearchEnabled() ? std::addressof(*filtered_entries_) : &entries_;
  int index = 0;

  // Fill list with entries
  for (const auto& entry : *tmp) {
    bool is_highlighted = highlighted_ ? highlighted_->playlist == entry.playlist.name : false;

    // Add playlist
    menu_entries.push_back(CreateEntry(index++, entry.playlist.name, is_highlighted, true,
                                       " [" + std::to_string(entry.playlist.songs.size()) + "]"));

    if (!entry.collapsed) continue;

    // Add songs
    for (const auto& song : entry.playlist.songs) {
      is_highlighted = highlighted_ ? highlighted_->playlist == entry.playlist.name &&
                                          (highlighted_->GetTitle() == song.GetTitle())
                                    : false;
      menu_entries.push_back(CreateEntry(index++, song.GetTitle(), is_highlighted, false));
    }
  }

  menu_entries.push_back(ftxui::filler());

  ftxui::Elements content{
      ftxui::vbox(menu_entries) | ftxui::reflect(Box()) | ftxui::frame | ftxui::flex,
  };

  // Append search box, if enabled
  if (IsSearchEnabled()) {
    content.push_back(RenderSearch());
    content.push_back(ftxui::text(""));
  }

  return ftxui::vbox(content) | ftxui::flex;
}

/* ********************************************************************************************** */

bool PlaylistMenu::OnEventImpl(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  // Collapse menu to hide songs from playlist
  if (event == Keybind::ArrowLeft || event == Keybind::Left) {
    return ToggleActivePlaylist(CollapseState::ForceClose);
  }

  // Collapse menu to show songs from playlist
  if (event == Keybind::ArrowRight || event == Keybind::Right) {
    return ToggleActivePlaylist(CollapseState::ForceOpen);
  }

  // Toggle collapse state from menu
  if (event == Keybind::Space) {
    return ToggleActivePlaylist(CollapseState::Toggle);
  }

  // Enable search mode
  if (!IsSearchEnabled() && event == keybinding::Navigation::EnableSearch) {
    EnableSearch();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

int PlaylistMenu::GetSizeImpl() const {
  int size = 0;

  const InternalPlaylists* const tmp =
      IsSearchEnabled() ? std::addressof(*filtered_entries_) : &entries_;

  for (const auto& entry : *tmp) {
    // playlist name + songs size
    size += 1 + (entry.collapsed ? entry.playlist.songs.size() : 0);
  }

  return size;
}

/* ********************************************************************************************** */

std::string PlaylistMenu::GetActiveEntryAsTextImpl() const {
  int count = 0;
  int selected = GetSelected();

  const InternalPlaylists* const tmp =
      IsSearchEnabled() ? std::addressof(*filtered_entries_) : &entries_;

  for (const auto& entry : *tmp) {
    if (count == selected) return entry.playlist.name;

    // Already checked playlist index, so increment count
    ++count;

    // Just skip if playlist is not collapsed
    if (!entry.collapsed) continue;

    for (const auto& song : entry.playlist.songs) {
      if (count == selected) return song.GetTitle();

      // Already checked song index, so increment count
      ++count;
    }
  }

  ERROR("Could not find an active entry");
  return "";
}

/* ********************************************************************************************** */

bool PlaylistMenu::OnClickImpl() {
  auto active = IsSearchEnabled() ? GetActivePlaylistFromSearch() : GetActivePlaylistFromNormal();

  if (!active.has_value()) return false;

  return on_click_(active);
}

/* ********************************************************************************************** */

void PlaylistMenu::FilterEntriesBy(const std::string& text) {
  // Do not even try to filter
  if (text.empty()) {
    filtered_entries_ = entries_;
    return;
  }

  filtered_entries_->clear();

  // Filter entries (try to match any of these: playlist title or song title)
  for (const auto& entry : entries_) {
    bool contains_text = util::contains(entry.playlist.name, text);

    // Create temporary playlist
    InternalPlaylist tmp{
        .collapsed = true,
        .playlist = model::Playlist{.index = entry.playlist.index, .name = entry.playlist.name},
    };

    // Append only filtered songs
    for (const auto& song : entry.playlist.songs) {
      if (util::contains(song.GetTitle(), text)) {
        tmp.playlist.songs.push_back(song);
        contains_text = true;
      }
    }

    // Only add if playlist name or song (artist/title) contains the given text
    if (contains_text) filtered_entries_->push_back(tmp);
  }
}

/* ********************************************************************************************** */

void PlaylistMenu::SetEntriesImpl(const model::Playlists& entries) {
  LOG("Set a new list of entries with size=", entries.size());
  if (!entries_.empty()) entries_.clear();

  entries_.reserve(entries.size());

  int count = 0;
  for (const auto& playlist : entries) {
    auto tmp = InternalPlaylist{
        .collapsed = false,
        .playlist =
            model::Playlist{
                .index = count++,
                .name = playlist.name,
                .songs = playlist.songs,
            },
    };
    entries_.push_back(tmp);
  }
}

/* ********************************************************************************************** */

void PlaylistMenu::EmplaceImpl(const model::Playlist& entry) {
  LOG("Emplace a new entry to list");
  model::Playlist new_entry = model::Playlist{
      .index = static_cast<int>(entries_.size()),
      .name = entry.name,
      .songs = entry.songs,
  };

  entries_.emplace_back(InternalPlaylist{
      .collapsed = false,
      .playlist = new_entry,
  });
}

/* ********************************************************************************************** */

void PlaylistMenu::UpdateOrEmplaceImpl(const model::Playlist& entry) {
  LOG("Update/emplace entry to list");
  bool found = false;

  for (auto& internal_entry : entries_) {
    // If modified playlist is based on an existing one, just replace it
    if (internal_entry.playlist.index == entry.index) {
      LOG("Changing old playlist=", internal_entry.playlist, " to new playlist=", entry);
      internal_entry.playlist = entry;
      found = true;
    }
  }

  // Otherwise, create a new entry for it
  if (!found) {
    LOG("Could not find a matching playlist, so create a new one");
    model::Playlist new_entry{
        .index = static_cast<int>(entries_.size()),
        .name = entry.name,
        .songs = entry.songs,
    };

    entries_.emplace_back(InternalPlaylist{
        .collapsed = false,
        .playlist = new_entry,
    });
  }
}

/* ********************************************************************************************** */

void PlaylistMenu::EraseImpl(const model::Playlist& entry) {
  LOG("Attempt to erase an entry with value=", entry);
  auto it = std::find_if(entries_.begin(), entries_.end(),
                         [&entry](const InternalPlaylist& p) { return p.playlist == entry; });

  if (it != entries_.end()) {
    LOG("Found matching entry, erasing it, entry=", it->playlist);
    entries_.erase(it);
  }
}

/* ********************************************************************************************** */

void PlaylistMenu::SetEntryHighlightedImpl(const model::Song& entry) {
  int index = 0;
  int count = 0;
  bool found = false;

  for (auto& tmp : entries_) {
    // Count playlist name
    if (!found) ++index;
    ++count;

    if (tmp.playlist.name != entry.playlist) {
      // Count all songs from this playlist
      if (tmp.collapsed) {
        index += !found ? tmp.playlist.songs.size() : 0;
        count += tmp.playlist.songs.size();
      }

      // As the playlist does not match, we just skip it
      continue;
    }

    for (auto& song : tmp.playlist.songs) {
      // We check if songs are equal based only on the streaming URL or filepath
      if (song.Compare(entry)) {
        // Always collapse playlist
        tmp.collapsed = true;

        // From now on, we cannot increment index anymore
        highlighted_ = entry;
        song = entry;
        found = true;
      }

      // Only increment if did not find song yet
      if (!found) ++index;
      ++count;
    }
  }

  if (!found) {
    LOG("Could not find entry to highlight");
    return;
  }

  // Resize boxes vector
  auto& boxes = GetBoxes();
  boxes.resize(count);

  // To get a better experience, update focused and select indexes,
  // to highlight current playing song entry in playlist
  ResetState(index);

  // And check for animation effect
  UpdateActiveEntry();
}

/* ********************************************************************************************** */

std::optional<model::Playlist> PlaylistMenu::GetActiveEntryImpl() const {
  int count = 0;
  int selected = GetSelected();

  const InternalPlaylists* const tmp =
      IsSearchEnabled() ? std::addressof(*filtered_entries_) : &entries_;

  for (const auto& entry : *tmp) {
    // Selected index is the playlist itself, so just return
    if (count == selected) return entry.playlist;

    // Already checked playlist index, so increment count
    ++count;

    if (!entry.collapsed) continue;

    // Otherwise, selected index may be pointing to a song entry
    for (auto it = entry.playlist.songs.begin(); it != entry.playlist.songs.end(); ++it, ++count) {
      if (count == selected) {
        // Get full playlist
        return entry.playlist;
      }
    }
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

bool PlaylistMenu::ToggleActivePlaylist(const CollapseState& state) {
  if (IsSearchEnabled()) return false;

  int selected = *GetSelected();
  int count = 0, total_size = 0;

  InternalPlaylist* internal_playlist = nullptr;

  for (auto& entry : entries_) {
    if (count == selected) {
      internal_playlist = &entry;
    }

    // Already checked playlist index, so increment both counters
    total_size += 1 + (entry.collapsed ? entry.playlist.songs.size() : 0);
    ++count;

    // As we do not care about song entries, just increment count
    if (entry.collapsed) count += entry.playlist.songs.size();

    // Do not attempt to find the selected playlist anymore
    if (count > selected) break;
  }

  if (!internal_playlist) return false;

  if (state == CollapseState::Toggle ||
      (state == CollapseState::ForceOpen && !internal_playlist->collapsed) ||
      (state == CollapseState::ForceClose && internal_playlist->collapsed)) {
    internal_playlist->collapsed = !internal_playlist->collapsed;
    LOG(internal_playlist->collapsed ? "Show" : "Hide",
        " collapsed playlist=", std::quoted(internal_playlist->playlist.name));

    // Resize vector of boxes to new size and check if must enable text animation
    Clamp();
    UpdateActiveEntry();
  }

  return true;
}

/* ********************************************************************************************** */

std::optional<model::Playlist> PlaylistMenu::GetActivePlaylistFromNormal() const {
  int count = 0;
  int selected = GetSelected();

  for (const auto& entry : entries_) {
    // Selected index is the playlist itself, so just return
    if (count == selected) return entry.playlist;

    // Already checked playlist index, so increment count
    ++count;

    if (!entry.collapsed) continue;

    // Otherwise, selected index may be pointing to a song entry
    for (auto it = entry.playlist.songs.begin(); it != entry.playlist.songs.end(); ++it, ++count) {
      // Selected index is a song from this playlist
      if (count == selected) {
        return ShufflePlaylist(entry.playlist, it);
      }
    }
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

std::optional<model::Playlist> PlaylistMenu::GetActivePlaylistFromSearch() const {
  // Do not even try
  if (!IsSearchEnabled()) return std::nullopt;

  // Playlist values from search
  int playlist_index = -1;
  std::string song_title;

  int count = 0;
  int selected = GetSelected();

  // Find original playlist index to query later in original list
  for (const auto& entry : *filtered_entries_) {
    // Selected index is the playlist itself, so just return
    if (count == selected) {
      playlist_index = entry.playlist.index;
      break;
    }

    // Already checked playlist index, so increment count
    ++count;

    if (!entry.collapsed) continue;

    // Otherwise, selected index may be pointing to a song entry
    for (auto it = entry.playlist.songs.begin(); it != entry.playlist.songs.end(); ++it, ++count) {
      // Selected index is a song from this playlist
      if (count == selected) {
        playlist_index = entry.playlist.index;
        song_title = it->GetTitle();
        break;
      }
    }

    // Just get out
    if (playlist_index != -1) break;
  }

  // With these values, find playlist in the original list of entries
  for (const auto& entry : entries_) {
    if (playlist_index != -1 && playlist_index == entry.playlist.index && song_title.empty()) {
      return entry.playlist;
    }

    if (playlist_index > entry.playlist.index) continue;

    // Otherwise, selected index may be pointing to a song entry
    for (auto it = entry.playlist.songs.begin(); it != entry.playlist.songs.end(); ++it) {
      if (playlist_index != -1 && playlist_index == entry.playlist.index &&
          it->GetTitle() == song_title) {
        return ShufflePlaylist(entry.playlist, it);
      }
    }
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

ftxui::Element PlaylistMenu::CreateEntry(int index, const std::string& text, bool is_highlighted,
                                         bool is_playlist, const std::string& suffix) {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  auto max_size = GetMaxColumns() ? ftxui::size(WIDTH, EQUAL, GetMaxColumns()) : ftxui::nothing;

  auto& boxes = GetBoxes();

  bool is_focused = (index == *GetFocused());
  bool is_selected = (index == *GetSelected());

  const auto& type = is_playlist
                         ? (is_highlighted ? styles_.playlist.playing : styles_.playlist.normal)
                         : (is_highlighted ? styles_.song.playing : styles_.song.normal);

  std::string prefix{is_selected ? "▶ " : "  "};
  auto prefix_text = ftxui::text(prefix);

  ftxui::Decorator style = is_selected ? (is_focused ? type.selected_focused : type.selected)
                                       : (is_focused ? type.focused : type.normal);

  auto focus_management = is_focused ? ftxui::select : ftxui::nothing;

  // In case of entry text too long, animation thread will be running, so we gotta take the
  // text content from there
  auto entry_text =
      ftxui::text(IsAnimationRunning() && is_selected ? GetTextFromAnimation() : text + suffix);

  return ftxui::hbox({
             prefix_text | styles_.prefix,
             ftxui::text(!is_playlist ? "  " : "") | style,
             entry_text | style | ftxui::xflex,
         }) |
         max_size | focus_management | ftxui::reflect(boxes[index]);
}

/* ********************************************************************************************** */

model::Playlist PlaylistMenu::ShufflePlaylist(
    const model::Playlist& playlist, const std::deque<model::Song>::const_iterator& it) const {
  // Do nothing
  if (it == playlist.songs.end()) {
    return playlist;
  }

  // Shuffle playlist based on selected song
  model::Playlist tmp{
      .index = playlist.index,
      .name = playlist.name,
      .songs = std::deque<model::Song>(it, playlist.songs.end()),
  };

  tmp.songs.insert(tmp.songs.end(), playlist.songs.begin(), it);
  return tmp;
}

}  // namespace internal
}  // namespace interface
