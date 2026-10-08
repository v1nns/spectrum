/**
 * \file
 * \brief Dummy class for stream fetcher support
 */

#ifndef INCLUDE_DEBUG_DUMMY_STREAM_FETCHER_H_
#define INCLUDE_DEBUG_DUMMY_STREAM_FETCHER_H_

#include <atomic>
#include <string>
#include <vector>

#include "model/application_error.h"
#include "model/song.h"
#include "web/base/stream_fetcher.h"

namespace driver {

/**
 * @brief Dummy implementation
 */
class DummyStreamFetcher : public web::StreamFetcher {
 public:
  /**
   * @brief Construct a new DummyStreamFetcher object
   */
  DummyStreamFetcher() = default;

  /**
   * @brief Destroy the DummyStreamFetcher object
   */
  virtual ~DummyStreamFetcher() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Initialize internal structures for stream fetcher
   */
  void Init() override {}

  /**
   * @brief Finish and clean up all internal structures from stream fetcher
   */
  void Finish() override {}

  /**
   * @brief Extract streaming information from the given URL
   * @param song Song with a streaming URL, fetching operation will get the rest of the info (out)
   * @return Error code from operation
   */
  error::Code ExtractInfo(model::Song& song) override { return error::kSuccess; }

  /**
   * @brief Forget any information kept from the given song
   * @return Always false, as nothing is kept
   */
  bool Forget(const model::Song& song) override { return false; }

  /**
   * @brief Check if stream fetcher is available
   * @return Always true
   */
  static bool IsAvailable() { return true; }

  /**
   * @brief Extract list of songs from the given playlist URL
   * @return Always an error, as there is nothing to extract
   */
  static error::Code ExtractPlaylist(const std::string&, std::vector<model::Song>&,
                                     const std::atomic<bool>* = nullptr) {
    return error::kStreamFetchFailed;
  }
};

}  // namespace driver
#endif  // INCLUDE_DEBUG_DUMMY_STREAM_FETCHER_H_
