/**
 * \file
 * \brief Interface class for URL fetching support
 */

#ifndef INCLUDE_AUDIO_LYRIC_BASE_URL_FETCHER_H_
#define INCLUDE_AUDIO_LYRIC_BASE_URL_FETCHER_H_

#include <string>

#include "model/application_error.h"

namespace driver {

/**
 * @brief Common interface to fetch content from URL
 */
class UrlFetcher {
 public:
  /**
   * @brief Construct a new UrlFetcher object
   */
  UrlFetcher() = default;

  /**
   * @brief Destroy the UrlFetcher object
   */
  virtual ~UrlFetcher() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Fetch content from the given URL
   * @param URL Endpoint address
   * @param output Output from fetch (out)
   * @return Error code from operation
   */
  virtual error::Code Fetch(const std::string &URL, std::string &output) = 0;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_LYRIC_BASE_URL_FETCHER_H_
