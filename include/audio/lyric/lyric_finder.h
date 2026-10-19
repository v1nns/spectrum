/**
 * \file
 * \brief  Class for Lyric Finder
 */

#ifndef INCLUDE_AUDIO_LYRIC_LYRIC_FINDER_H_
#define INCLUDE_AUDIO_LYRIC_LYRIC_FINDER_H_

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "audio/lyric/search_config.h"
#include "model/song.h"
#include "web/base/html_parser.h"
#include "web/base/url_fetcher.h"

#ifdef ENABLE_TESTS
namespace {
class LyricFinderTest;
}
#endif

namespace lyric {

/**
 * @brief Result from searching song lyrics
 */
struct SearchResult {
  //! Possible outcomes from search
  enum class Status : std::uint8_t {
    Found,        //!< Song lyrics found
    NotFound,     //!< Every search engine was reached, but none of them contains song lyrics
    FetchFailed,  //!< Could not fetch content from some search engine (e.g. network error or busy
                  //!< server), and the others do not contain song lyrics
  };

  Status status = Status::NotFound;  //!< Search outcome
  model::SongLyric lyrics;           //!< Song lyrics (filled only when found)
  model::SyncedLyric synced;         //!< Synced song lyrics (filled only when found, if available)
  uint32_t duration = 0;  //!< Duration in seconds of the song which lyrics belong to (0 if unknown)
};

/**
 * @brief Responsible to fetch content from search engines and web scrap song lyrics from it
 */
class LyricFinder {
  /**
   * @brief Construct a new LyricFinder object
   * @param fetcher Pointer to URL fetcher interface
   * @param parser Pointer to HTML parser interface
   */
  explicit LyricFinder(std::unique_ptr<web::UrlFetcher>&& fetcher,
                       std::unique_ptr<web::HtmlParser>&& parser);

 protected:
  /**
   * @brief Construct a new LyricFinder object
   */
  LyricFinder() = default;

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return LyricFinder object
   * @param fetcher Pass fetcher to be used within LyricFinder (optional)
   * @param parser Pass parser to be used within LyricFinder (optional)
   * @return std::unique_ptr<LyricFinder> LyricFinder instance
   */
  static std::unique_ptr<LyricFinder> Create(web::UrlFetcher* fetcher = nullptr,
                                             web::HtmlParser* parser = nullptr);

  /**
   * @brief Destroy the LyricFinder object
   */
  virtual ~LyricFinder() = default;

  //! Remove these
  LyricFinder(const LyricFinder& other) = delete;             // copy constructor
  LyricFinder(LyricFinder&& other) = delete;                  // move constructor
  LyricFinder& operator=(const LyricFinder& other) = delete;  // copy assignment
  LyricFinder& operator=(LyricFinder&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Search for lyrics by fetching the search engine and web scraping it
   * @param artist Artist name
   * @param title Song name
   * @param duration Song duration in seconds (0 if unknown)
   * @return Search result, containing song lyrics (if found)
   */
  virtual SearchResult Search(const std::string& artist, const std::string& title,
                              uint32_t duration);

  /**
   * @brief Set function to check if search in progress must be canceled (checked before each
   * search engine, and also while fetching content from it)
   * @param check Function returning true to cancel search
   */
  void SetCancelCheck(const web::UrlFetcher::CancelCheck& check);

  /* ******************************************************************************************** */
  //! Internal methods
 private:
  /**
   * @brief Remove from song title everything that is not part of song name, which usually comes
   * from video titles: sections split by "|" containing the artist or video-related words, and
   * featured artists or video-related words, e.g. "(feat. X)" or "[Official Video]"
   * @param artist Artist name
   * @param title Song title
   * @return Song title to use in search
   */
  static std::string CleanTitle(const std::string& artist, const std::string& title);

  /**
   * @brief Get first artist from song credits, e.g. "Artist" from "Artist, Other" or "Artist & B"
   * @param artist Artist name (may contain more than one artist)
   * @return First artist name (same as given when it contains a single artist)
   */
  static std::string FirstArtist(const std::string& artist);

  /**
   * @brief Search for song lyrics on each search engine, until one of them has it
   * @param artist Artist name
   * @param title Song title (already cleaned)
   * @param duration Song duration in seconds that lyrics must match, using only search engines
   * that check it (0 to not check it, using every search engine)
   * @return Search result
   */
  SearchResult SearchEngines(const std::string& artist, const std::string& title,
                             uint32_t duration = 0);

  /**
   * @brief Search for synced lyrics from another version of the song (e.g. a live recording), with
   * duration close enough to song duration, on search engines that support it
   * @param artist Artist name
   * @param title Song title (already cleaned)
   * @param duration Song duration in seconds
   * @return Synced song lyrics (empty if not found)
   */
  model::SyncedLyric SearchSyncedVersion(const std::string& artist, const std::string& title,
                                         uint32_t duration);

  /**
   * @brief Fetch content from URL, retrying a few times (waiting longer each time) while server is
   * busy or temporarily unavailable
   * @param url Endpoint address
   * @param output Output from fetch (out)
   * @return Error code from last attempt
   */
  error::Code FetchWithRetry(const std::string& url, std::string& output);

  /**
   * @brief Wait before retrying to fetch content, stopping as soon as search is canceled
   * @param delay Time to wait
   * @return true if waited the whole time, false if search was canceled
   */
  bool WaitBeforeRetry(std::chrono::milliseconds delay) const;

  /* ******************************************************************************************** */
  //! Variables
 private:
  //! Maximum difference in seconds between durations of the song playing and the one which synced
  //! lyrics belong to (e.g. a live recording has its lines sung at other times than studio version)
  static constexpr uint32_t kMaxSyncedDurationDifference = 10;

  static constexpr int kMaxRetries = 2;  //!< Retries while server is busy or unavailable
  static constexpr std::chrono::milliseconds kRetryDelay{1000};  //!< Wait before first retry
  static constexpr std::chrono::milliseconds kCancelCheckInterval{100};  //!< While waiting retry

  Config engines_ = SearchConfig::Create();   //!< Search engine settings
  std::unique_ptr<web::UrlFetcher> fetcher_;  //!< URL fetcher
  std::unique_ptr<web::HtmlParser> parser_;   //!< HTML parser

  web::UrlFetcher::CancelCheck cancel_check_;  //!< Check if search in progress must be canceled
  std::chrono::milliseconds retry_delay_ = kRetryDelay;  //!< Wait before first retry (doubles)

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::LyricFinderTest;
#endif
};

}  // namespace lyric
#endif  // INCLUDE_AUDIO_LYRIC_LYRIC_FINDER_H_
