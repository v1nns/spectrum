/**
 * \file
 * \brief  Structures for results from searching an artist (on MusicBrainz or YouTube)
 */

#ifndef INCLUDE_MODEL_ARTIST_SEARCH_H_
#define INCLUDE_MODEL_ARTIST_SEARCH_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace model {

/**
 * @brief Artist found on MusicBrainz
 */
struct Artist {
  std::string id;              //!< MusicBrainz identifier (MBID)
  std::string name;            //!< Artist name
  std::string type;            //!< Artist type (e.g. "Person" or "Group", may be empty)
  std::string country;         //!< Country code (e.g. "BR", may be empty)
  std::string disambiguation;  //!< Comment to tell apart artists with the same name (may be empty)

  //! Overloaded operators
  friend bool operator==(const Artist& lhs, const Artist& rhs) {
    return lhs.id == rhs.id && lhs.name == rhs.name && lhs.type == rhs.type &&
           lhs.country == rhs.country && lhs.disambiguation == rhs.disambiguation;
  }
};

/**
 * @brief Release group from an artist found on MusicBrainz (an album, EP or single, grouping all
 * of its editions), whose tracks are listed only when needed
 */
struct ReleaseGroup {
  std::string id;            //!< MusicBrainz identifier (MBID)
  std::string title;         //!< Release title
  std::string date;          //!< First release date ("YYYY[-MM[-DD]]", may be empty)
  std::string primary_type;  //!< Main type (e.g. "Album", "EP" or "Single")
  std::vector<std::string> secondary_types;  //!< Other types (e.g. "Live" or "Compilation")

  //! Overloaded operators
  friend bool operator==(const ReleaseGroup& lhs, const ReleaseGroup& rhs) {
    return lhs.id == rhs.id && lhs.title == rhs.title && lhs.date == rhs.date &&
           lhs.primary_type == rhs.primary_type && lhs.secondary_types == rhs.secondary_types;
  }
};

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
#endif  // INCLUDE_MODEL_ARTIST_SEARCH_H_
