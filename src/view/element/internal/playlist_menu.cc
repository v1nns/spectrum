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
    : Menu(dispatcher, force_refresh), on_click_{on_click} {}

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
    menu_entries.push_back(CreateEntry(index++, entry.playlist.name, is_highlighted, true));

    if (!entry.collapsed) continue;

    // Add songs
    for (const auto& song : entry.playlist.songs) {
      is_highlighted = highlighted_ ? highlighted_->playlist == entry.playlist.name &&
                                          highlighted_->filepath == song.filepath
                                    : false;
      menu_entries.push_back(
          CreateEntry(index++, song.filepath.filename().string(), is_highlighted, false));
    }
  }

  menu_entries.push_back(ftxui::filler());

  ftxui::Elements content{
      ftxui::vbox(menu_entries) | ftxui::reflect(GetBox()) | ftxui::frame | ftxui::flex,
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
    return ToggleActivePlaylist(/*collapse=*/false);
  }

  // Collapse menu to show songs from playlist
  if (event == Keybind::ArrowRight || event == Keybind::Right) {
    return ToggleActivePlaylist(/*collapse=*/true);
  }

  // Enable search mode
  if (!IsSearchEnabled() && event == keybinding::Playlist::EnableSearch) {
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
      if (count == selected) return song.filepath.filename().string();

      // Already checked song index, so increment count
      ++count;
    }
  }

  ERROR("Could not find an active entry");
  return "";
}

/* ********************************************************************************************** */

bool PlaylistMenu::OnClickImpl() {
  auto active = IsSearchEnabled() ? GetActivePlaylistFromSearch() : GetActivePlaylist();

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

  // Filter entries (try to match any of these: playlist title or song filepath)
  for (const auto& entry : entries_) {
    bool contains_text = util::contains(entry.playlist.name, text);

    // Create temporary playlist
    InternalPlaylist tmp{
        .index = entry.index,
        .collapsed = true,
        .playlist = model::Playlist{.name = entry.playlist.name},
    };

    // Append only filtered songs
    for (const auto& song : entry.playlist.songs) {
      if (util::contains(song.filepath.filename().string(), text)) {
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
    entries_.push_back(InternalPlaylist{
        .index = count++,
        .collapsed = false,
        .playlist = playlist,
    });
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

      continue;
    }

    for (const auto& song : tmp.playlist.songs) {
      // We check only by the filepath, otherwise it will never be equal
      if (song.filepath == entry.filepath) {
        // Always collapse playlist
        tmp.collapsed = true;

        // From now on, we cannot increment index anymore
        highlighted_ = entry;
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
        // Get only playlist + selected song
        model::Playlist playlist{
            .name = entry.playlist.name,
            .songs = std::deque<model::Song>({*it}),
        };

        return playlist;
      }
    }
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

ftxui::Element PlaylistMenu::CreateEntry(int index, const std::string& text, bool is_highlighted,
                                         bool is_playlist) {
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
      ftxui::text(IsAnimationRunning() && is_selected ? GetTextFromAnimation() : text);

  return ftxui::hbox({
             prefix_text | styles_.prefix,
             ftxui::text(!is_playlist ? "  " : "") | style,
             entry_text | style | ftxui::xflex,
         }) |
         max_size | focus_management | ftxui::reflect(boxes[index]);
}

/* ********************************************************************************************** */

bool PlaylistMenu::ToggleActivePlaylist(bool collapse) {
  if (IsSearchEnabled()) return false;

  int selected = *GetSelected();
  int count = 0, total_size = 0;
  bool toggled = false;

  for (auto& entry : entries_) {
    if (count == selected) {
      LOG(collapse ? "Show" : "Hide", " collapsed playlist ", std::quoted(entry.playlist.name));
      entry.collapsed = collapse;
      toggled = true;
    }

    // Already checked playlist index, so increment both counters
    total_size += 1 + (entry.collapsed ? entry.playlist.songs.size() : 0);
    ++count;

    // As we do not care about song entries, just increment count
    if (entry.collapsed) count += entry.playlist.songs.size();
  }

  // Playlist state was toggled
  if (toggled) {
    // Resize vector of boxes to new size
    auto& boxes = GetBoxes();
    boxes.resize(total_size);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

std::optional<model::Playlist> PlaylistMenu::GetActivePlaylist() const {
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
  std::filesystem::path song;

  int count = 0;
  int selected = GetSelected();

  // Find original playlist index to query later in original list
  for (const auto& entry : *filtered_entries_) {
    // Selected index is the playlist itself, so just return
    if (count == selected) {
      playlist_index = entry.index;
      break;
    }

    // Already checked playlist index, so increment count
    ++count;

    if (!entry.collapsed) continue;

    // Otherwise, selected index may be pointing to a song entry
    for (auto it = entry.playlist.songs.begin(); it != entry.playlist.songs.end(); ++it, ++count) {
      // Selected index is a song from this playlist
      if (count == selected) {
        playlist_index = entry.index;
        song = it->filepath;
        break;
      }
    }

    // Just get out
    if (playlist_index != -1) break;
  }

  // With these values, find playlist in the original list of entries
  for (const auto& entry : entries_) {
    if (playlist_index != -1 && playlist_index == entry.index && song.empty()) {
      return entry.playlist;
    }

    if (playlist_index > entry.index) continue;

    // Otherwise, selected index may be pointing to a song entry
    for (auto it = entry.playlist.songs.begin(); it != entry.playlist.songs.end(); ++it) {
      if (playlist_index != -1 && playlist_index == entry.index && it->filepath == song) {
        return ShufflePlaylist(entry.playlist, it);
      }
    }
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

model::Playlist PlaylistMenu::ShufflePlaylist(
    const model::Playlist& playlist, const std::deque<model::Song>::const_iterator& it) const {
  // Do nothing
  if (it == playlist.songs.end()) {
    return playlist;
  }

  // Shuffle playlist based on selected song
  model::Playlist tmp{.name = playlist.name,
                      .songs = std::deque<model::Song>(it, playlist.songs.end())};

  tmp.songs.insert(tmp.songs.end(), playlist.songs.begin(), it);
  return tmp;
}

}  // namespace internal
}  // namespace interface
