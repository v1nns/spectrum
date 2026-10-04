/**
 * \file
 * \brief  Base class for a playlist
 */

#ifndef INCLUDE_MODEL_PLAYLIST_H_
#define INCLUDE_MODEL_PLAYLIST_H_

#include <deque>
#include <ostream>
#include <vector>

#include "model/song.h"

namespace model {

/**
 * @brief List of audio files to play
 */
struct Playlist {
  int index;               //!< Index identifier
  std::string name;        //!< Playlist name
  std::deque<Song> songs;  //!< List of songs

  //! Overloaded operators
  friend std::ostream& operator<<(std::ostream& out, const Playlist& p);
  friend bool operator==(const Playlist& lhs, const Playlist& rhs);
  friend bool operator!=(const Playlist& lhs, const Playlist& rhs);

  /**
   * @brief Check if song list is empty
   * @return True if empty, otherwise false
   */
  bool IsEmpty() const { return songs.empty(); }

  //! Pretty-print for testing
  friend void PrintTo(const Playlist& p, std::ostream* os);
};

using Playlists = std::vector<Playlist>;

}  // namespace model
#endif  // INCLUDE_MODEL_PLAYLIST_H_
