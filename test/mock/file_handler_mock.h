/**
 * \file
 * \brief  Mock class for File Handler API
 */

#ifndef INCLUDE_TEST_MOCK_FILE_HANDLER_MOCK_H_
#define INCLUDE_TEST_MOCK_FILE_HANDLER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "model/playlist.h"
#include "util/file_handler.h"

namespace {

class FileHandlerMock final : public util::FileHandler {
 public:
  MOCK_METHOD(bool, ParsePlaylists, (model::Playlists & playlists), (override));
  MOCK_METHOD(bool, SavePlaylists, (const model::Playlists& playlists), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_FILE_HANDLER_MOCK_H_
