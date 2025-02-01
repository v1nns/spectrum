/**
 * \file
 * \brief Interface class for stream fetcher support
 */

#ifndef INCLUDE_WEB_BASE_STREAM_FETCHER_H_
#define INCLUDE_WEB_BASE_STREAM_FETCHER_H_

#include <string>

#include "model/application_error.h"
#include "model/song.h"

namespace web {

/**
 * @brief Common interface to get streaming information from given URL
 */
class StreamFetcher {
 public:
  /**
   * @brief Construct a new StreamFetcher object
   */
  StreamFetcher() = default;

  /**
   * @brief Destroy the StreamFetcher object
   */
  virtual ~StreamFetcher() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Extract streaming information from the given URL
   * @param url Endpoint address
   * @param output Song filled with streaming information from fetch operation (out)
   * @return Error code from operation
   */
  // TODO: think about removing url and use only song
  virtual error::Code ExtractInfo(const std::string &url, model::Song &output) = 0;
};

}  // namespace web
#endif  // INCLUDE_WEB_BASE_STREAM_FETCHER_H_
