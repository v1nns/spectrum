/**
 * \file
 * \brief Interface class for URL fetching support
 */

#ifndef INCLUDE_WEB_BASE_URL_FETCHER_H_
#define INCLUDE_WEB_BASE_URL_FETCHER_H_

#include <functional>
#include <string>
#include <utility>

#include "model/application_error.h"

namespace web {

/**
 * @brief Common interface to fetch content from URL
 */
class UrlFetcher {
 public:
  //! Function to check if fetch in progress must be canceled (returns true to cancel it)
  using CancelCheck = std::function<bool()>;

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
   * @param url Endpoint address
   * @param output Output from fetch (out)
   * @return Error code from operation
   */
  virtual error::Code Fetch(const std::string &url, std::string &output) = 0;

  /**
   * @brief Set function to check if fetch in progress must be canceled (implementation may ignore
   * it, in case it does not support cancellation)
   * @param check Function returning true to cancel fetch
   */
  void SetCancelCheck(CancelCheck check) { cancel_check_ = std::move(check); }

  /* ******************************************************************************************** */
  //! Variables
 protected:
  CancelCheck cancel_check_;  //!< Check if fetch in progress must be canceled
};

}  // namespace web
#endif  // INCLUDE_WEB_BASE_URL_FETCHER_H_
