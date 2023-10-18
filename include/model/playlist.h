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
  std::string name;        //!< Playlist name
  std::deque<Song> songs;  //!< List of songs

  //! Overloaded operators
  friend std::ostream& operator<<(std::ostream& out, const Playlist& p);
  bool operator==(const Playlist& other) const;
  bool operator!=(const Playlist& other) const;

  /**
   * @brief Check if song list is empty
   * @return True if empty, otherwise false
   */
  bool IsEmpty() const { return songs.empty(); }

  /**
   * @brief Returns the first element from the inner deque container with songs
   * @warning Be sure to check first if deque contains at least one song
   * @return First song in the deque
   */
  Song PopFront();
};

using Playlists = std::vector<Playlist>;

}  // namespace model
#endif  // INCLUDE_MODEL_PLAYLIST_H_
