/**
 * \file
 * \brief  Class to wrap yt-dlp funcionalities
 */

#ifndef INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_
#define INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model/application_error.h"
#include "model/song.h"
#include "nlohmann/json_fwd.hpp"
#include "util/logger.h"
#include "web/base/stream_fetcher.h"

#ifdef ENABLE_TESTS
namespace {
class YtDlpWrapperTest;
}
#endif

namespace driver {

/**
 * @brief Class to run yt-dlp (as an external program, if available on PATH) and extract streaming
 * information from the given URL
 */
class YtDlpWrapper : public web::StreamFetcher {
  //! Program used to extract information from URL (only searched in PATH)
  static constexpr std::string_view kProgram = "yt-dlp";

  //! Maximum time to wait for program to extract information (it fetches content from network)
  static constexpr std::chrono::seconds kTimeout{60};

 public:
  /**
   * @brief Construct a new YtDlpWrapper object
   */
  YtDlpWrapper() = default;

  /**
   * @brief Destroy the YtDlpWrapper object
   */
  virtual ~YtDlpWrapper() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Initialize internal structures for stream fetcher
   */
  void Init() override;

  /**
   * @brief Finish and clean up all internal structures from stream fetcher
   */
  void Finish() override;

  /**
   * @brief Extract streaming information from the given URL
   * @param song Song with a streaming URL, fetching operation will get the rest of the info (out)
   * @return Error code from operation
   */
  error::Code ExtractInfo(model::Song& song) override;

  /**
   * @brief Forget any information kept from the given song, so it is fetched again by the next
   * call to extract it (e.g. when its streaming URL is not accepted anymore)
   * @param song Song with a streaming URL
   * @return true if the last information extracted for this song was not fetched at that moment
   * (it was the one kept from a previous call), otherwise false
   */
  bool Forget(const model::Song& song) override;

  /**
   * @brief Check if yt-dlp can be found (needed to extract information from URL)
   * @return true if it is available in PATH, otherwise false
   */
  static bool IsAvailable();

  /**
   * @brief Extract list of songs from the given YouTube playlist URL (only their URL and title,
   * the rest is extracted when each song is played)
   * @param url YouTube playlist URL
   * @param songs List of songs from playlist (out)
   * @param cancel Flag to cancel extraction while it is running (optional)
   * @return Error code from operation
   */
  static error::Code ExtractPlaylist(const std::string& url, std::vector<model::Song>& songs,
                                     const std::atomic<bool>* cancel = nullptr);

  /* ******************************************************************************************** */
  //! Internal methods
 private:
  //! Maximum number of songs with information kept
  static constexpr size_t kMaxSongsKept = 64;

  //! Streaming URL is not used when it is this close to expire (besides the time to play song)
  static constexpr std::chrono::seconds kExpirationMargin{120};

  //! Information extracted for a song, which is kept while its streaming URL may be used, to not
  //! ask for it every time that the same song is played
  struct SongKept {
    std::string artist;
    std::string title;
    uint16_t num_channels = 0;
    uint32_t bit_rate = 0;
    uint32_t duration = 0;
    model::StreamInfo stream_info;

    std::chrono::seconds expiration{0};  //!< When streaming URL expires (since epoch)
    bool reused = false;                 //!< Last call to extract it did not fetch anything
  };

  /**
   * @brief Get the moment when the given streaming URL expires, which is informed by itself
   * @param streaming_url URL for audio stream
   * @return Moment when URL expires, as seconds since epoch (or nothing, if it is not informed)
   */
  static std::optional<std::chrono::seconds> GetExpiration(const std::string& streaming_url);

  //! Keep information extracted for the given song (only when its streaming URL expires)
  void Keep(const model::Song& song);

  /**
   * @brief Fill song with the information kept for it, if its streaming URL may still be used
   * @param song Song with a streaming URL (out)
   * @return true if song was filled, otherwise false
   */
  bool Reuse(model::Song& song);

  /**
   * @brief Find out why program failed, based on what it printed as error
   * @param error Content written by program to standard error
   * @return Error code for the reason found (or the one for a generic failure)
   */
  static error::Code GetFailureReason(const std::string& error);

  /**
   * @brief Fill song with information extracted by yt-dlp (title, duration and the best audio
   * stream to play)
   * @param info JSON parsed output from yt-dlp
   * @param song Song information (out)
   * @return Error code from operation (when there is no audio stream to play, for example)
   */
  error::Code ParseInfo(const nlohmann::json& info, model::Song& song);

  /**
   * @brief Fill list of songs with entries from playlist extracted by yt-dlp (skipping entries that
   * cannot be played, like deleted or private videos)
   * @param info JSON parsed output from yt-dlp (using flat playlist)
   * @param songs List of songs (out)
   * @return Error code from operation (when output does not contain a playlist, for example)
   */
  static error::Code ParsePlaylist(const nlohmann::json& info, std::vector<model::Song>& songs);

  /**
   * @brief Fill artist and title, parsed from video title (as "Artist - Title"). If video title
   * does not contain an artist, use (in this order) artist, uploader or channel from metadata
   * @param title Video title
   * @param metadata JSON parsed metadata from video
   * @param song Song information
   */
  static void FillArtistAndTitle(const std::string& title, const nlohmann::json& metadata,
                                 model::Song& song);

  /**
   * @brief Fill streaming information inside Song structure with content from parsed JSON
   * @param entry JSON parsed entry
   * @param duration Song duration (in seconds)
   * @param song Song information
   */
  void FillStreamInfo(const nlohmann::json& entry, uint32_t duration, model::Song& song);

  /**
   * @brief Select best audio stream to play, preferring (in this order): direct HTTP streams
   * (instead of HLS playlists), original language (instead of dubbed audio), audio without dynamic
   * range compression, and then the highest quality and bitrate
   * @param streams JSON list with all audio-only formats extracted by yt-dlp
   * @return Pointer to selected entry from list (or nullptr, if none of them has an URL)
   */
  static const nlohmann::json* SelectStream(const nlohmann::json& streams);

  //! Information kept for each song (by its original URL)
  std::map<std::string, SongKept, std::less<>> songs_kept_;

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::YtDlpWrapperTest;
#endif
};

}  // namespace driver
#endif  // INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_
