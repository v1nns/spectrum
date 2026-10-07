#include "middleware/remote_playlist.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iterator>
#include <string_view>
#include <system_error>

#include "model/song.h"
#include "model/stream_info.h"
#include "view/element/internal/file_menu.h"

namespace middleware {

namespace {

constexpr int kQueueIndex = -1;  //!< Index for a list of songs that is not a saved playlist

//! How an URL starts
constexpr std::array<std::string_view, 2> kUrlPrefixes{"http://", "https://"};

/* ********************************************************************************************** */

//! Check if it is a file that may be played
bool IsMediaFile(const util::File& file) {
  std::error_code error;

  return !std::filesystem::is_directory(file, error) &&
         interface::internal::FileMenu::HasMediaExtension(file);
}

/* ********************************************************************************************** */

//! Add media file to list of songs
void AddSong(model::Playlist& playlist, const util::File& file) {
  if (IsMediaFile(file)) playlist.songs.push_back(model::Song{.filepath = file});
}

}  // namespace

/* ********************************************************************************************** */

bool IsRemoteUrl(const std::string& target) {
  return std::any_of(kUrlPrefixes.begin(), kUrlPrefixes.end(), [&target](std::string_view prefix) {
    return target.compare(0, prefix.size(), prefix) == 0;
  });
}

/* ********************************************************************************************** */

std::optional<model::Playlist> CreateRemotePlaylist(const std::string& target,
                                                    util::FileHandler& file_handler) {
  model::Playlist playlist{.index = kQueueIndex};

  if (IsRemoteUrl(target)) {
    playlist.songs.push_back(model::Song{.stream_info = model::StreamInfo{.base_url = target}});
    return playlist;
  }

  const std::filesystem::path path{target};
  std::error_code error;

  if (path.is_absolute() && std::filesystem::exists(path, error)) {
    const bool is_directory = std::filesystem::is_directory(path, error);

    // Files are listed in the same order shown by UI
    util::Files files;
    if (!file_handler.ListFiles(is_directory ? path : path.parent_path(), files))
      return std::nullopt;

    if (is_directory) {
      for (const auto& file : files) AddSong(playlist, file);
    } else {
      // Even without a known extension, the file asked is played (as UI also does)
      playlist.songs.push_back(model::Song{.filepath = path});

      // Starting from this file, every other media file from its directory is played once
      const auto selected = std::find(files.begin(), files.end(), path);

      if (selected != files.end()) {
        std::for_each(std::next(selected), files.end(),
                      [&playlist](const util::File& file) { AddSong(playlist, file); });
        std::for_each(files.begin(), selected,
                      [&playlist](const util::File& file) { AddSong(playlist, file); });
      }
    }

    if (playlist.IsEmpty()) return std::nullopt;
    return playlist;
  }

  // Otherwise, it is the name of a playlist
  model::Playlists saved;
  if (!file_handler.ParsePlaylists(saved)) return std::nullopt;

  const auto found =
      std::find_if(saved.begin(), saved.end(),
                   [&target](const model::Playlist& entry) { return entry.name == target; });

  if (found == saved.end() || found->IsEmpty()) return std::nullopt;
  return *found;
}

}  // namespace middleware
