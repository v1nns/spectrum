/**
 * \file
 * \brief  Mock class for Decoder API
 */

#ifndef INCLUDE_TEST_DECODER_MOCK_H_
#define INCLUDE_TEST_DECODER_MOCK_H_

#include "audio/base/decoder.h"
#include "model/song.h"

namespace {

class DecoderMock final : public driver::Decoder {
 public:
  MOCK_METHOD(error::Code, OpenFile, (model::Song * audio_info), (override));
  MOCK_METHOD(error::Code, Decode, (int samples, AudioCallback callback), (override));
  MOCK_METHOD(void, ClearCache, (), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_DECODER_MOCK_H_
