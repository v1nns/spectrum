/**
 * \file
 * \brief  Mock class for Playback API
 */

#ifndef INCLUDE_TEST_PLAYBACK_MOCK_H_
#define INCLUDE_TEST_PLAYBACK_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "audio/base/playback.h"

namespace {

class PlaybackMock final : public audio::Playback {
 public:
  MOCK_METHOD(error::Code, CreatePlaybackStream, (const std::string&), (override));
  MOCK_METHOD(model::AudioDevices, ListDevices, (), (const, override));
  MOCK_METHOD(std::string, GetDevice, (), (const, override));
  MOCK_METHOD(error::Code, ConfigureParameters, (const model::AudioFormat&), (override));
  MOCK_METHOD(model::AudioFormat, GetFormat, (), (const, override));
  MOCK_METHOD(error::Code, Prepare, (), (override));
  MOCK_METHOD(error::Code, Pause, (), (override));
  MOCK_METHOD(error::Code, Stop, (), (override));
  MOCK_METHOD(error::Code, AudioCallback, (void*, int), (override));
  MOCK_METHOD(error::Code, SetVolume, (model::Volume), (override));
  MOCK_METHOD(model::Volume, GetVolume, (), (override));
  MOCK_METHOD(uint32_t, GetPeriodSize, (), (const override));
};

}  // namespace
#endif  // INCLUDE_TEST_PLAYBACK_MOCK_H_
