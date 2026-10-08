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
  MOCK_METHOD(void, DequeueNextSong, (), (override));
  MOCK_METHOD(void, SkipToNext, (), (override));
  MOCK_METHOD(void, SkipToPrevious, (), (override));
  MOCK_METHOD(void, SetRepeatMode, (model::RepeatMode), (override));
  MOCK_METHOD(void, SetShuffle, (bool), (override));
  MOCK_METHOD(void, SetAudioDevice, (const std::string&), (override));
  MOCK_METHOD(model::AudioDevices, GetAudioDevices, (), (const, override));
  MOCK_METHOD(void, Exit, (), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_AUDIO_CONTROL_MOCK_H_
