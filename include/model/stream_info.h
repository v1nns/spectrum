/**
 * \file
 * \brief  Base class for streaming information
 */

#ifndef INCLUDE_MODEL_STREAM_INFO_H_
#define INCLUDE_MODEL_STREAM_INFO_H_

#include <cstdint>
#include <map>
#include <ostream>
#include <string>

namespace model {

/**
 * @brief Detailed audio metadata information from song
 */
struct StreamInfo {
  std::string codec;        //!< Audio codec used (e.g., "mp4a.40.2", "opus")
  std::string extension;    //!< File extension associated with the audio (e.g., ".m4a", ".webm")
  uint64_t filesize = 0;    //!< Size of the audio file in bytes
  std::string description;  //!< Human-readable description of the metadata

  std::string base_url;       //!< Original URL (e.g., "https://www.youtube.com/watch?v=...")
  std::string streaming_url;  //!< URL for HTTP audio stream

  std::map<std::string, std::string>
      http_header;  //!< Additional information to use on HTTP request to stream audio

  //! Overloaded operators
  friend std::ostream& operator<<(std::ostream& out, const StreamInfo& s);
  friend bool operator==(const StreamInfo& lhs, const StreamInfo& rhs);
  friend bool operator!=(const StreamInfo& lhs, const StreamInfo& rhs);
};

/**
 * @brief Util method to pretty print StreamInfo structure
 * @param arg StreamInfo struct
 * @return std::string Formatted string with properties from StreamInfo
 */
std::string to_string(const StreamInfo& arg);

}  // namespace model
#endif  // INCLUDE_MODEL_STREAM_INFO_H_
