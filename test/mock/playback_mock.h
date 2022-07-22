/**
 * \file
 * \brief  Mock class for Playback API
 */

#ifndef INCLUDE_TEST_PLAYBACK_MOCK_H_
#define INCLUDE_TEST_PLAYBACK_MOCK_H_

#include "audio/base/playback.h"

namespace {

class PlaybackMock final : public driver::Playback {
 public:
  MOCK_METHOD(error::Code, CreatePlaybackStream, (), (override));
  MOCK_METHOD(error::Code, ConfigureParameters, (), (override));
  MOCK_METHOD(error::Code, Prepare, (), (override));
  MOCK_METHOD(error::Code, Pause, (), (override));
  MOCK_METHOD(error::Code, Stop, (), (override));
  MOCK_METHOD(error::Code, AudioCallback, (void* buffer, int max_size, int actual_size),
              (override));
  MOCK_METHOD(uint32_t, GetPeriodSize, (), (const override));
};

}  // namespace
#endif  // INCLUDE_TEST_PLAYBACK_MOCK_H_
