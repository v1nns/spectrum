/**
 * \file
 * \brief  Class to wrap yt-dlp funcionalities
 */

#ifndef INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_
#define INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model/application_error.h"
#include "model/artist_search.h"
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

  //! Option for program to read cookies from a browser and send them to site
  static constexpr std::string_view kOptionCookies = "--cookies-from-browser";

  //! Maximum songs imported from a YouTube mix (it never ends, so program would list hundreds of
  //! them, while YouTube itself shows only a few dozen)
  static constexpr int kMixMaxSongs = 50;

  //! Maximum channels listed when searching for an artist (the first ones are the most relevant)
  static constexpr int kSearchMaxChannels = 8;

  //! Maximum playlists listed when searching for albums from an artist
  static constexpr int kSearchMaxPlaylists = 15;

  //! Base URL to search YouTube (followed by the text to search)
  static constexpr std::string_view kSearchUrl = "https://www.youtube.com/results?search_query=";

  //! Filters added to search URL, to find only channels or only playlists
  static constexpr std::string_view kChannelsFilter = "&sp=EgIQAg%3D%3D";
  static constexpr std::string_view kPlaylistsFilter = "&sp=EgIQAw%3D%3D";

  //! Maximum programs running at the same time to find songs (more of them would make YouTube
  //! refuse requests sooner, taking them as coming from a bot)
  static constexpr int kMaxParallelSearches = 3;

  //! Maximum results listed when searching for a single song
  static constexpr int kSongMaxResults = 3;

  //! Maximum difference from expected duration for a video to be taken as the song searched
  static constexpr uint32_t kMaxDurationDifference = 10;

  //! Base URL to search songs on YouTube Music (followed by the text to search and section)
  static constexpr std::string_view kMusicSearchUrl = "https://music.youtube.com/search?q=";
  static constexpr std::string_view kMusicSongsSection = "#songs";

  //! Prefix to search videos on YouTube, listing a few results (followed by the text to search)
  static constexpr std::string_view kVideoSearchPrefix = "ytsearch3:";

  //! Interval to check if searching songs must be canceled
  static constexpr std::chrono::milliseconds kCancelCheckInterval{50};

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

  /**
   * @brief Search YouTube for channels from the given artist. Topic channels (e.g. "Artist -
   * Topic") are skipped, as they do not list any release, and verified channels come first
   * @param query Artist name
   * @param channels List of channels found (out)
   * @param cancel Flag to cancel search while it is running (optional)
   * @return Error code from operation
   */
  static error::Code SearchArtists(const std::string& query,
                                   std::vector<model::YoutubeChannel>& channels,
                                   const std::atomic<bool>* cancel = nullptr);

  /**
   * @brief Extract releases (albums, EPs and singles) from the given channel, as listed by YouTube
   * @param channel_id Channel identifier
   * @param releases List of releases (out), empty when channel does not have any
   * @param cancel Flag to cancel extraction while it is running (optional)
   * @return Error code from operation
   */
  static error::Code ExtractReleases(const std::string& channel_id,
                                     std::vector<model::YoutubeRelease>& releases,
                                     const std::atomic<bool>* cancel = nullptr);

  /**
   * @brief Search YouTube for playlists with albums from the given artist (created by anyone, used
   * when artist has no channel with releases)
   * @param query Artist name
   * @param playlists List of playlists found (out)
   * @param cancel Flag to cancel search while it is running (optional)
   * @return Error code from operation
   */
  static error::Code SearchPlaylists(const std::string& query,
                                     std::vector<model::YoutubeRelease>& playlists,
                                     const std::atomic<bool>* cancel = nullptr);

  /**
   * @brief Find each song on YouTube, from its artist, title and duration (e.g. songs from an album
   * listed by MusicBrainz). Song is first searched on YouTube Music (whose results are audio only,
   * matched by title), then on YouTube itself (matched by duration). Searches are split between a
   * few programs running at the same time, and all of them stop on the first refused request
   * @param songs Songs to find (out), whose streaming URL is filled when found (songs that already
   * have one are skipped)
   * @param progress Number of songs already searched (out, optional)
   * @param cancel Flag to cancel search while it is running (optional)
   * @return Error code from operation (success even when some songs are not found)
   */
  static error::Code MatchSongs(std::vector<model::Song>& songs,
                                std::atomic<int>* progress = nullptr,
                                const std::atomic<bool>* cancel = nullptr);

  /**
   * @brief Set browser whose cookies are sent to site when it refuses requests (asking for a login
   * to prove that they do not come from a bot). Cookies are read by yt-dlp itself, and only after a
   * request is refused; from then on, they are sent by every request. It must be set before any
   * information is extracted (as it is shared by all instances)
   * @param browser Browser as expected by yt-dlp (BROWSER[+KEYRING][:PROFILE][::CONTAINER]), or
   * empty to never send any cookies
   */
  static void SetCookiesFromBrowser(const std::string& browser);

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
   * @brief Run program to extract information from URL. When site refuses the request and there is
   * a browser to read cookies from, program is executed once more to send them
   * @param program Full path to program
   * @param options Options for program (URLs are always the last arguments)
   * @param urls URLs to extract information from (each one printed in a line, when it is JSON)
   * @param output Content written by program to standard output (out)
   * @param cancel Flag to cancel program while it is running (optional)
   * @return Error code from operation (with the reason for program to fail, when it is known)
   */
  static error::Code Run(const std::string& program, const std::vector<std::string>& options,
                         const std::vector<std::string>& urls, std::string& output,
                         const std::atomic<bool>* cancel = nullptr);

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
   * @brief Fill list of channels with entries from search extracted by yt-dlp (skipping Topic
   * channels and anything else that is not a channel), placing verified ones first
   * @param info JSON parsed output from yt-dlp (using flat playlist)
   * @param channels List of channels (out)
   * @return Error code from operation (when output does not contain a list, for example)
   */
  static error::Code ParseChannels(const nlohmann::json& info,
                                   std::vector<model::YoutubeChannel>& channels);

  /**
   * @brief Fill list of releases with playlists extracted by yt-dlp (skipping anything else that
   * is not a playlist, like a channel found by search)
   * @param info JSON parsed output from yt-dlp (using flat playlist)
   * @param releases List of releases (out)
   * @return Error code from operation (when output does not contain a list, for example)
   */
  static error::Code ParseReleases(const nlohmann::json& info,
                                   std::vector<model::YoutubeRelease>& releases);

  /**
   * @brief Run program to list entries from the given URL (without extracting information from
   * each one of them) and parse its output
   * @param url URL to list entries from
   * @param max_entries Maximum number of entries listed (0 for no limit)
   * @param parse Function to parse program output
   * @param cancel Flag to cancel program while it is running (optional)
   * @return Error code from operation
   */
  static error::Code ListEntries(const std::string& url, int max_entries,
                                 const std::function<error::Code(const nlohmann::json&)>& parse,
                                 const std::atomic<bool>* cancel);

  /**
   * @brief Run program once to search all the given texts (or URLs from a search), listing a few
   * results for each one of them
   * @param program Full path to program
   * @param searches URLs (or prefixed texts) to search
   * @param results Results for each search, by its URL (out)
   * @param cancel Flag to cancel program while it is running (optional)
   * @return Error code from operation
   */
  static error::Code Search(const std::string& program, const std::vector<std::string>& searches,
                            std::map<std::string, nlohmann::json>& results,
                            const std::atomic<bool>* cancel);

  /**
   * @brief Pick the result whose title is the same as the song title (ignoring case and
   * typographic punctuation, and then also text in brackets from only one of them, like
   * "(Remastered)")
   * @param entries Results from search
   * @param title Song title
   * @return Identifier of the video picked (or nothing, if none of them has the same title)
   */
  static std::optional<std::string> PickByTitle(const nlohmann::json& entries,
                                                const std::string& title);

  /**
   * @brief Pick the result whose duration is the closest to song duration (up to a few seconds of
   * difference), skipping live versions when song is not one
   * @param entries Results from search
   * @param title Song title
   * @param duration Song duration (in seconds)
   * @return Identifier of the video picked (or nothing, if none of them is close enough)
   */
  static std::optional<std::string> PickByDuration(const nlohmann::json& entries,
                                                   const std::string& title, uint32_t duration);

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

  static inline std::string cookies_browser_;           //!< Browser to read cookies from (if any)
  static inline std::atomic<bool> use_cookies_{false};  //!< A request was refused without cookies

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::YtDlpWrapperTest;
#endif
};

}  // namespace driver
#endif  // INCLUDE_WEB_DRIVER_YTDLP_WRAPPER_H_
