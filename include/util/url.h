/**
 * \file
 * \brief  Utilities for URL validation and formatting
 */

#ifndef INCLUDE_UTIL_URL_H_
#define INCLUDE_UTIL_URL_H_

#include <regex>
#include <string>

namespace util {

/**
 * @brief Basic validation for YouTube URL (same rule used when parsing playlists file)
 * @param url URL to validate
 * @return true if URL points to YouTube, false otherwise
 */
inline bool IsYoutubeUrl(const std::string& url) {
  static const std::regex valid_url(R"(^(https?://)?(www\.)?(?:youtube\.com|youtu\.be)/.*$)");
  return std::regex_match(url, valid_url);
}

/**
 * @brief Check if URL is from a YouTube playlist, including URL from a video that belongs to a
 * playlist (e.g. "https://www.youtube.com/watch?v=dQw4w9WgXcQ&list=...")
 * @param url URL to check
 * @return true if URL points to a YouTube playlist, false otherwise
 */
inline bool IsYoutubePlaylistUrl(const std::string& url) {
  static const std::regex playlist(R"([?&]list=[\w-]+)");
  return IsYoutubeUrl(url) && std::regex_search(url, playlist);
}

/**
 * @brief Get a shorter version of the given YouTube URL to display on UI (e.g.
 * "https://www.youtube.com/watch?v=dQw4w9WgXcQ" turns into "youtu.be/dQw4w9WgXcQ")
 * @param url YouTube URL
 * @return Short URL if video identifier was found, otherwise URL without scheme and "www."
 */
inline std::string ShortenYoutubeUrl(const std::string& url) {
  static const std::regex video_id(R"((?:youtu\.be/|[?&]v=|/shorts/)([\w-]{11}))");
  static const std::regex prefix(R"(^(https?://)?(www\.)?)");

  if (std::smatch match; std::regex_search(url, match, video_id)) {
    return "youtu.be/" + match[1].str();
  }

  return std::regex_replace(url, prefix, "");
}

}  // namespace util
#endif  // INCLUDE_UTIL_URL_H_
