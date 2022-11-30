/**
 * \file
 * \brief  Mock class for Analyzer API
 */

#ifndef INCLUDE_TEST_ANALYZER_MOCK_H_
#define INCLUDE_TEST_ANALYZER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "audio/base/analyzer.h"

namespace {

class AnalyzerMock final : public driver::Analyzer {
 public:
  MOCK_METHOD(error::Code, Init, (int output_size), (override));
  MOCK_METHOD(error::Code, Execute, (double *in, int size, double *out), (override));
  MOCK_METHOD(int, GetBufferSize, (), (override));
  MOCK_METHOD(int, GetOutputSize, (), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_ANALYZER_MOCK_H_