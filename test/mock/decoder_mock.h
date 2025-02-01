/**
 * \file
 * \brief  Mock class for Decoder API
 */

#ifndef INCLUDE_TEST_MOCK_DECODER_MOCK_H_
#define INCLUDE_TEST_MOCK_DECODER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "audio/base/decoder.h"
#include "model/song.h"
#include "model/volume.h"

namespace {

class DecoderMock final : public audio::Decoder {
 public:
  MOCK_METHOD(error::Code, Open, (model::Song &), (override));
  MOCK_METHOD(error::Code, Decode, (int, AudioCallback), (override));
  MOCK_METHOD(void, ClearCache, (), (override));
  MOCK_METHOD(error::Code, SetVolume, (model::Volume), (override));
  MOCK_METHOD(model::Volume, GetVolume, (), (const, override));
  MOCK_METHOD(error::Code, UpdateFilters, (const model::EqualizerPreset &), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_DECODER_MOCK_H_
