/**
 * \file
 * \brief  Mock class for URL fetching API
 */

#ifndef INCLUDE_TEST_URL_FETCHER_MOCK_H_
#define INCLUDE_TEST_URL_FETCHER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "web/base/url_fetcher.h"

namespace {

class UrlFetcherMock final : public web::UrlFetcher {
 public:
  MOCK_METHOD(error::Code, Fetch, (const std::string&, std::string&), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_URL_FETCHER_MOCK_H_
