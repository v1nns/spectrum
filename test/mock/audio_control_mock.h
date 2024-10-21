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
  MOCK_METHOD(void, Play, (const std::filesystem::path&), (override));
  MOCK_METHOD(void, Play, (const model::Playlist&), (override));
  MOCK_METHOD(void, PauseOrResume, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, SetAudioVolume, (const model::Volume&), (override));
  MOCK_METHOD(model::Volume, GetAudioVolume, (), (const, override));
  MOCK_METHOD(void, SeekForwardPosition, (int value), (override));
  MOCK_METHOD(void, SeekBackwardPosition, (int value), (override));
  MOCK_METHOD(void, ApplyAudioFilters, (const model::EqualizerPreset&), (override));
  MOCK_METHOD(void, Exit, (), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_AUDIO_CONTROL_MOCK_H_
