/**
 * \file
 * \brief  Class to search artists and their releases on MusicBrainz
 */

#ifndef INCLUDE_WEB_MUSICBRAINZ_H_
#define INCLUDE_WEB_MUSICBRAINZ_H_

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "model/application_error.h"
#include "model/artist_search.h"
#include "model/song.h"
#include "nlohmann/json_fwd.hpp"
#include "web/base/url_fetcher.h"

#ifdef ENABLE_TESTS
namespace {
class MusicBrainzTest;
}
#endif

namespace web {

/**
 * @brief Search artists, their releases (with release date) and the tracks from each release on
 * MusicBrainz, an open music encyclopedia (https://musicbrainz.org/doc/MusicBrainz_API). Requests
 * are sent one at a time and at most one per second, as asked by its usage rules
 */
class MusicBrainz {
  //! Base URL for every request
  static constexpr std::string_view kBaseUrl = "https://musicbrainz.org/ws/2/";

  //! User-Agent asked by MusicBrainz, naming the application and a way to reach its developers
  static constexpr std::string_view kUserAgent = "spectrum ( https://github.com/v1nns/spectrum )";

  //! Maximum artists listed when searching (the first ones are the most relevant)
  static constexpr int kMaxArtists = 8;

  //! Maximum entries per request (the maximum accepted by MusicBrainz)
  static constexpr int kPageSize = 100;

  //! Maximum requests to list releases from an artist (each one with a page of them)
  static constexpr int kMaxPages = 5;

  //! Maximum editions listed to pick the one with tracks (each one with all of its tracks)
  static constexpr int kMaxEditions = 25;

  //! Minimum interval between two requests
  static constexpr std::chrono::milliseconds kRequestInterval{1100};

  //! Interval before trying again when server is busy
  static constexpr std::chrono::milliseconds kRetryInterval{2000};

  //! Maximum attempts for each request (server replies as busy when requests are too frequent)
  static constexpr int kMaxAttempts = 3;

  //! Interval to check if waiting must be canceled
  static constexpr std::chrono::milliseconds kCancelCheckInterval{50};

  /**
   * @brief Construct a new MusicBrainz object
   * @param fetcher URL fetcher
   */
  explicit MusicBrainz(std::unique_ptr<UrlFetcher>&& fetcher);

 public:
  /**
   * @brief Factory method: Create new MusicBrainz instance
   * @param fetcher URL fetcher (ownership is transferred; if null, a default one is created)
   * @return std::shared_ptr<MusicBrainz> MusicBrainz instance
   */
  static std::shared_ptr<MusicBrainz> Create(UrlFetcher* fetcher = nullptr);

  /**
   * @brief Destroy the MusicBrainz object
   */
  ~MusicBrainz() = default;

  //! Remove these
  MusicBrainz(const MusicBrainz& other) = delete;             // copy constructor
  MusicBrainz(MusicBrainz&& other) = delete;                  // move constructor
  MusicBrainz& operator=(const MusicBrainz& other) = delete;  // copy assignment
  MusicBrainz& operator=(MusicBrainz&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Search artists with the given name (most relevant first)
   * @param query Artist name
   * @param artists List of artists found (out)
   * @param cancel Flag to cancel search while it is running (optional)
   * @return Error code from operation
   */
  error::Code SearchArtists(const std::string& query, std::vector<model::Artist>& artists,
                            const std::atomic<bool>* cancel = nullptr);

  /**
   * @brief List albums, EPs and singles from the given artist (including live albums,
   * compilations and so on, which are told apart by their secondary types)
   * @param artist_id Artist identifier
   * @param releases List of releases (out)
   * @param cancel Flag to cancel listing while it is running (optional)
   * @return Error code from operation
   */
  error::Code FetchReleases(const std::string& artist_id,
                            std::vector<model::ReleaseGroup>& releases,
                            const std::atomic<bool>* cancel = nullptr);

  /**
   * @brief List tracks from the given release, as in its earliest official edition with the most
   * common track list (only title and duration are filled)
   * @param release_id Release group identifier
   * @param tracks List of tracks (out)
   * @param cancel Flag to cancel listing while it is running (optional)
   * @return Error code from operation
   */
  error::Code FetchTracks(const std::string& release_id, std::vector<model::Song>& tracks,
                          const std::atomic<bool>* cancel = nullptr);

  /* ******************************************************************************************** */
  //! Internal methods
 private:
  /**
   * @brief Send request to MusicBrainz (waiting for the interval since the last one, and trying
   * again while server is busy) and parse its content
   * @param path Path and parameters after base URL (without the format parameter)
   * @param output Parsed content (out)
   * @param cancel Flag to cancel request (optional)
   * @return Error code from operation
   */
  error::Code Request(const std::string& path, nlohmann::json& output,
                      const std::atomic<bool>* cancel);

  /**
   * @brief Wait for the given time
   * @param duration Time to wait
   * @param cancel Flag to stop waiting (optional)
   * @return true if waited until the end, false if canceled
   */
  static bool Wait(std::chrono::steady_clock::duration duration, const std::atomic<bool>* cancel);

  /**
   * @brief Fill list of artists with content from search
   * @param content Parsed content
   * @param artists List of artists (out)
   * @return Error code from operation (when content is not the expected one)
   */
  static error::Code ParseArtists(const nlohmann::json& content,
                                  std::vector<model::Artist>& artists);

  /**
   * @brief Append releases from a page of them to the given list
   * @param content Parsed content
   * @param releases List of releases (out)
   * @param total Number of releases from artist, considering all pages (out)
   * @return Error code from operation (when content is not the expected one)
   */
  static error::Code ParseReleases(const nlohmann::json& content,
                                   std::vector<model::ReleaseGroup>& releases, int& total);

  /**
   * @brief Fill list of tracks with the ones from earliest edition of release with the most common
   * track list (skipping promotional editions with only a few tracks, or deluxe ones)
   * @param content Parsed content (with editions listed and their tracks)
   * @param tracks List of tracks (out)
   * @return Error code from operation (when content is not the expected one)
   */
  static error::Code ParseTracks(const nlohmann::json& content, std::vector<model::Song>& tracks);

  /* ******************************************************************************************** */
  //! Variables

  std::unique_ptr<UrlFetcher> fetcher_;  //!< Fetch content from URL

  std::mutex mutex_;                                    //!< Send one request at a time
  std::chrono::steady_clock::time_point last_request_;  //!< When the last request was sent

  std::chrono::milliseconds request_interval_ = kRequestInterval;  //!< Interval between requests
  std::chrono::milliseconds retry_interval_ = kRetryInterval;      //!< Interval to try again

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::MusicBrainzTest;
#endif
};

}  // namespace web
#endif  // INCLUDE_WEB_MUSICBRAINZ_H_
