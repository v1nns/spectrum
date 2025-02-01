/**
 * \file
 * \brief  Class to wrap CURL funcionalities
 */

#ifndef INCLUDE_WEB_DRIVER_CURL_WRAPPER_H_
#define INCLUDE_WEB_DRIVER_CURL_WRAPPER_H_

#include <curl/curl.h>

#include <memory>
#include <string>
#include <string_view>

#include "model/application_error.h"
#include "web/base/url_fetcher.h"

namespace driver {

/**
 * @brief Class to manage CURL resources and perform content fetching from the given URL
 */
class CURLWrapper : public web::UrlFetcher {
  // The Accept request HTTP header indicates which content types, expressed as MIME types, the
  // client is able to understand
  static constexpr std::string_view kAcceptType =
      "Accept:text/html,application/xhtml+xml,application/xml";

  // The User-Agent request header is a characteristic string that lets servers and network peers
  // identify the application, operating system, vendor and version of the requesting user agent.
  static constexpr std::string_view kUserAgent =
      "User-Agent:Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.17 (KHTML, like Gecko) "
      "Chrome/24.0.1312.70 Safari/537.17";

 public:
  /**
   * @brief Fetch content from the given URL
   * @param url Endpoint address
   * @param output Output from fetch (out)
   * @return Error code from operation
   */
  error::Code Fetch(const std::string &url, std::string &output) override;

 private:
  /**
   * @brief This callback function gets called by libcurl as soon as there is data received that
   * needs to be saved. For most transfers, this callback gets called many times and each invoke
   * delivers another chunk of data.
   * @param buffer Pointer to delivered data
   * @param size Value always equal to 1
   * @param nmemb Size of data
   * @param data Output buffer
   * @return Real size from received data
   */
  static size_t WriteCallback(const char *buffer, size_t size, size_t nmemb, void *data);

  //! Smart pointer to manage CURL resource
  using CURLGuard = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
};

}  // namespace driver
#endif  // INCLUDE_WEB_DRIVER_CURL_WRAPPER_H_
