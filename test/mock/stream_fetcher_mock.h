/**
 * \file
 * \brief  Mock class for Stream Fetcher API
 */

#ifndef INCLUDE_TEST_MOCK_STREAM_FETCHER_MOCK_H_
#define INCLUDE_TEST_MOCK_STREAM_FETCHER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "web/base/stream_fetcher.h"

namespace {

class StreamFetcherMock final : public web::StreamFetcher {
 public:
  MOCK_METHOD(void, Init, (), (override));
  MOCK_METHOD(void, Finish, (), (override));
  MOCK_METHOD(error::Code, ExtractInfo, (model::Song &), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_STREAM_FETCHER_MOCK_H_
