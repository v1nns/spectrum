/**
 * \file
 * \brief  Mock class for lyric finder API
 */

#ifndef INCLUDE_TEST_LYRIC_FINDER_MOCK_H_
#define INCLUDE_TEST_LYRIC_FINDER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "audio/lyric/lyric_finder.h"
#include "model/song.h"

namespace {

class LyricFinderMock final : public lyric::LyricFinder {
 public:
  MOCK_METHOD(model::SongLyric, Search, (const std::string&, const std::string&), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_LYRIC_FINDER_MOCK_H_
