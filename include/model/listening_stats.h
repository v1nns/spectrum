/**
 * \file
 * \brief  Statistics about what was listened to (kept between runs)
 */

#ifndef INCLUDE_MODEL_LISTENING_STATS_H_
#define INCLUDE_MODEL_LISTENING_STATS_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace model {

/**
 * @brief Totals of everything listened to, and of each song (there is no history of when each song
 * was played)
 */
struct ListeningStats {
  //! Maximum number of songs with totals kept (the least listened is removed to add another one)
  static constexpr std::size_t kMaxSongs = 500;

  //! Totals from a single song
  struct Song {
    std::string artist;        //!< Song artist (may be empty)
    std::string title;         //!< Song title (or filename/URL, when it has no title)
    uint64_t listened_ms = 0;  //!< Time listened to (in milliseconds)
    uint32_t plays = 0;        //!< Times that it started playing

    //! Overloaded operators
    bool operator==(const Song& other) const;
    bool operator!=(const Song& other) const { return !operator==(other); }
  };

  //! Totals from a single artist (sum of their songs)
  struct Artist {
    std::string name;          //!< Artist name
    uint64_t listened_ms = 0;  //!< Time listened to (in milliseconds)
  };

  int64_t since = 0;  //!< When the first song was played (seconds since epoch, zero if none yet)

  uint64_t listened_ms = 0;  //!< Time listened to (in milliseconds), from files and URLs
  uint64_t local_ms = 0;     //!< Time listened to songs from files (in milliseconds)
  uint64_t stream_ms = 0;    //!< Time listened to songs from URL (in milliseconds)

  uint32_t songs_played = 0;    //!< Songs that started playing
  uint32_t songs_finished = 0;  //!< Songs played until their end
  uint32_t songs_skipped = 0;   //!< Songs stopped before their end (e.g. skipped, stopped by user)

  uint64_t stream_bytes = 0;  //!< Audio data read from URLs (in bytes)

  //! Totals from each song, identified by its filepath (or URL)
  std::map<std::string, Song> songs;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Count song that started playing
   * @param key Song identifier (filepath or URL)
   * @param artist Song artist
   * @param title Song title
   * @param now Current moment (seconds since epoch)
   */
  void AddPlayed(const std::string& key, const std::string& artist, const std::string& title,
                 int64_t now);

  /**
   * @brief Count time listened to a song (already counted as played)
   * @param key Song identifier (filepath or URL)
   * @param from_url Song is played from URL
   * @param ms Time listened to (in milliseconds)
   */
  void AddListened(const std::string& key, bool from_url, uint64_t ms);

  /**
   * @brief Check if nothing was listened to yet
   * @return true if there is no song played, false otherwise
   */
  bool IsEmpty() const { return songs_played == 0; }

  /**
   * @brief Get songs listened to the most (by time)
   * @param count Maximum number of songs
   * @return Songs sorted from the most listened one
   */
  std::vector<Song> GetTopSongs(std::size_t count) const;

  /**
   * @brief Get artists listened to the most (by time), songs without artist are not included
   * @param count Maximum number of artists
   * @return Artists sorted from the most listened one
   */
  std::vector<Artist> GetTopArtists(std::size_t count) const;

  //! Overloaded operators
  bool operator==(const ListeningStats& other) const;
  bool operator!=(const ListeningStats& other) const { return !operator==(other); }
};

}  // namespace model
#endif  // INCLUDE_MODEL_LISTENING_STATS_H_
