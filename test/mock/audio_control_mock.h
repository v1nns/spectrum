/**
 * \file
 * \brief  Mock class for Audio Control API
 */

#ifndef INCLUDE_TEST_MOCK_AUDIO_CONTROL_MOCK_H_
#define INCLUDE_TEST_MOCK_AUDIO_CONTROL_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "audio/player.h"

namespace {

class AudioControlMock final : public audio::AudioControl {
 public:
  MOCK_METHOD(void, Play, (const std::string& filepath), (override));
  MOCK_METHOD(void, PauseOrResume, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, SetAudioVolume, (model::Volume value), (override));
  MOCK_METHOD(model::Volume, GetAudioVolume, (), (override));
  MOCK_METHOD(void, SeekForwardPosition, (int value), (override));
  MOCK_METHOD(void, SeekBackwardPosition, (int value), (override));
  MOCK_METHOD(void, Exit, (), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_AUDIO_CONTROL_MOCK_H_
