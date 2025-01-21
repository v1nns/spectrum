/**
 * \file
 * \brief Dummy class for URL fetching support
 */

#ifndef INCLUDE_DEBUG_DUMMY_FETCHER_H_
#define INCLUDE_DEBUG_DUMMY_FETCHER_H_

#include <string>

namespace driver {

/**
 * @brief Dummy implementation
 */
class DummyFetcher : public UrlFetcher {
 public:
  /**
   * @brief Construct a new DummyFetcher object
   */
  DummyFetcher() = default;

  /**
   * @brief Destroy the DummyFetcher object
   */
  virtual ~DummyFetcher() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Fetch content from the given URL
   * @param URL Endpoint address
   * @param output Output from fetch (out)
   * @return Error code from operation
   */
  error::Code Fetch(const std::string &URL, std::string &output) override {
    return error::kSuccess;
  }
};

}  // namespace driver
#endif  // INCLUDE_DEBUG_DUMMY_FETCHER_H_
