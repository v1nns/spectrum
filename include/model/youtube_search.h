/**
 * \file
 * \brief  Structures for results from searching an artist on YouTube
 */

#ifndef INCLUDE_MODEL_YOUTUBE_SEARCH_H_
#define INCLUDE_MODEL_YOUTUBE_SEARCH_H_

#include <cstdint>
#include <optional>
#include <string>

namespace model {

/**
 * @brief YouTube channel found when searching for an artist
 */
struct YoutubeChannel {
  std::string id;                     //!< Channel identifier (e.g. "UCq19-LqvG35A-30oyAiPiqA")
  std::string name;                   //!< Channel name
  std::optional<uint64_t> followers;  //!< Number of followers (not informed by every channel)
  bool verified = false;              //!< Channel is verified by YouTube (e.g. official artist)

  //! Overloaded operators
  friend bool operator==(const YoutubeChannel& lhs, const YoutubeChannel& rhs) {
    return lhs.id == rhs.id && lhs.name == rhs.name && lhs.followers == rhs.followers &&
           lhs.verified == rhs.verified;
  }
};

/**
 * @brief Release from an artist (album, EP or single) or any playlist found on YouTube, whose songs
 * are listed only when needed
 */
struct YoutubeRelease {
  std::string id;        //!< Playlist identifier (e.g. "OLAK5uy_..." for an album)
  std::string title;     //!< Release or playlist title
  std::string uploader;  //!< Who created playlist (empty for releases from artist channel)

  //! Overloaded operators
  friend bool operator==(const YoutubeRelease& lhs, const YoutubeRelease& rhs) {
    return lhs.id == rhs.id && lhs.title == rhs.title && lhs.uploader == rhs.uploader;
  }
};

}  // namespace model
#endif  // INCLUDE_MODEL_YOUTUBE_SEARCH_H_
