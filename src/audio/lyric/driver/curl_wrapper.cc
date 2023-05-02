#include "audio/lyric/driver/curl_wrapper.h"

// #include <vector>

namespace driver {

error::Code CURLWrapper::Fetch(const std::string &URL, std::string &output) {
  SmartCURL curl(curl_easy_init(), curl_easy_cleanup);

  if (!curl) {
    // return CURLE_FAILED_INIT;
    return error::kUnknownError;
  }

  curl_easy_setopt(curl.get(), CURLOPT_URL, URL.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, CURLWrapper::WriteCallback);

  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &output);
  curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1);

  curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, kAcceptType.data());
  curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, kUserAgent.data());

  //   std::vector<char> err_buffer(CURL_ERROR_SIZE);
  //   curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, &err_buffer[0]);

  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_REFERER, URL.c_str());

  CURLcode result = curl_easy_perform(curl.get());

  if (result != CURLE_OK) {
    // TODO: log error
    //   std::string error(err_buffer.begin(), err_buffer.end());
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

size_t CURLWrapper::WriteCallback(char *buffer, size_t size, size_t nmemb, void *data) {
  size_t result = size * nmemb;
  static_cast<std::string *>(data)->append(buffer, result);
  return result;
}

}  // namespace driver