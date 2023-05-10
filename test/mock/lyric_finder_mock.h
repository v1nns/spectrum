/**
 * \file
 * \brief  Mock class for lyric finder API
 */

#ifndef INCLUDE_TEST_LYRIC_FINDER_MOCK_H_
#define INCLUDE_TEST_LYRIC_FINDER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "audio/lyric/lyric_finder.h"

namespace {

class LyricFinderMock final : public lyric::LyricFinder {
 public:
  MOCK_METHOD(lyric::SongLyric, Search, (const std::string&, const std::string&), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_LYRIC_FINDER_MOCK_H_
