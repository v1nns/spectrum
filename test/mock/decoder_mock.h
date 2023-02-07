/**
 * \file
 * \brief  Mock class for Decoder API
 */

#ifndef INCLUDE_TEST_MOCK_DECODER_MOCK_H_
#define INCLUDE_TEST_MOCK_DECODER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include <vector>

#include "audio/base/decoder.h"
#include "model/song.h"
#include "model/volume.h"

namespace {

class DecoderMock final : public driver::Decoder {
 public:
  MOCK_METHOD(error::Code, OpenFile, (model::Song & audio_info), (override));
  MOCK_METHOD(error::Code, Decode, (int samples, AudioCallback callback), (override));
  MOCK_METHOD(void, ClearCache, (), (override));
  MOCK_METHOD(error::Code, SetVolume, (model::Volume value), (override));
  MOCK_METHOD(model::Volume, GetVolume, (), (const, override));
  MOCK_METHOD(error::Code, UpdateFilters, (const std::vector<model::AudioFilter>& filters),
              (override));
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_DECODER_MOCK_H_
