/**
 * \file
 * \brief  Create a playlist from what was asked to play by a remote command
 */

#ifndef INCLUDE_MIDDLEWARE_REMOTE_PLAYLIST_H_
#define INCLUDE_MIDDLEWARE_REMOTE_PLAYLIST_H_

#include <optional>
#include <string>

#include "model/playlist.h"
#include "util/file_handler.h"

namespace middleware {

/**
 * @brief Check if target is an URL (to play it as a stream), instead of a path or a playlist name
 * @param target What was asked to play
 * @return true if it is an URL, otherwise false
 */
bool IsRemoteUrl(const std::string& target);

/**
 * @brief Create the list of songs to play for a target, which may be one of these (in this order):
 * - URL: a single song to stream;
 * - file: that file, followed by all other media files from its directory (same as UI does);
 * - directory: all media files inside it;
 * - name of a saved playlist: all songs from it.
 * @param target What was asked to play (a path must be absolute)
 * @param file_handler Utility handler to list files and read saved playlists
 * @return Playlist (or nothing, if there is no song to play for this target)
 */
std::optional<model::Playlist> CreateRemotePlaylist(const std::string& target,
                                                    util::FileHandler& file_handler);

}  // namespace middleware
#endif  // INCLUDE_MIDDLEWARE_REMOTE_PLAYLIST_H_
