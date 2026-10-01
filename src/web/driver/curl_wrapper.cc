#include "web/driver/curl_wrapper.h"

#include <vector>

#include "util/logger.h"

namespace driver {

error::Code CURLWrapper::Fetch(const std::string &url, std::string &output) {
  // Initialize cURL
  CURLGuard curl(curl_easy_init(), &curl_easy_cleanup);

  if (!curl) {
    ERROR("Failed to initialize cURL");
    return error::kUnknownError;
  }

  // Configure URL and write callback for response
  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, CURLWrapper::WriteCallback);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &output);

  // Set maximum timeout and disable any signal/alarm handlers
  curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, kConnectTimeout);
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, kTimeout);
  curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1);

  // Abort request as soon as owner asks for it
  if (cancel_check_) {
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, CURLWrapper::ProgressCallback);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
  }

  // Set header configuration for accept type and user agent
  curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, kAcceptType.data());
  curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, kUserAgent.data());

  // Configure buffer to write error message
  std::vector<char> err_buffer(CURL_ERROR_SIZE);
  curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, &err_buffer[0]);

  // Set up some configuration to avoid being ignored by any CGI (Common Gateway Interface)
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_REFERER, url.c_str());

  // Enable TLSv1.3 version only
  curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);

  LOG("Fetching content from URL=", url);
  if (CURLcode result = curl_easy_perform(curl.get()); result != CURLE_OK) {
    if (result == CURLE_ABORTED_BY_CALLBACK) {
      LOG("Canceled fetching content from URL=", url);
    } else {
      ERROR("Failed to execute cURL, error=", std::string(err_buffer.begin(), err_buffer.end()));
    }

    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

int CURLWrapper::ProgressCallback(void *data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  const auto *self = static_cast<CURLWrapper *>(data);
  return self->cancel_check_ && self->cancel_check_() ? 1 : 0;
}

/* ********************************************************************************************** */

size_t CURLWrapper::WriteCallback(const char *buffer, size_t size, size_t nmemb, void *data) {
  size_t result = size * nmemb;
  static_cast<std::string *>(data)->append(buffer, result);
  return result;
}

}  // namespace driver
