#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "audio/driver/fftw.h"
#include "model/application_error.h"
#include "util/logger.h"

namespace {

using ::testing::ElementsAreArray;
using ::testing::Matcher;

/**
 * @brief Tests with FFTW class
 */
class FftwTest : public ::testing::Test {
  // using-declarations
  using Fftw = std::unique_ptr<driver::FFTW>;

 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override { Init(); }

  void TearDown() override { analyzer.reset(); }

  void Init() {
    analyzer = std::make_unique<driver::FFTW>();
    analyzer->Init(kNumberBars * 2);
  }

  // TODO: implement (get block starting on line :78)
  void PrintResults(const std::vector<double>& result) {}

 protected:
  static constexpr int kNumberBars = 10;    //!< Number of bars per channel
  static constexpr int kBufferSize = 1024;  //!< Input buffer size

  Fftw analyzer;  //!< Audio frequency analysis
};

/* ********************************************************************************************** */

TEST_F(FftwTest, InitAndExecute) {
  // Create expected results
  const Matcher<double> expected_200MHz[kNumberBars] = {0, 0, 0.999, 0.009, 0, 0.001, 0, 0, 0, 0};
  const Matcher<double> expected_2000MHz[kNumberBars] = {0, 0, 0, 0, 0, 0, 0.524, 0.474, 0, 0};

  // Create in/out buffers
  int out_size = analyzer->GetOutputSize();
  std::vector<double> out(out_size, 0);
  std::vector<double> in(kBufferSize, 0);

  // Running execute 300 times (simulating about 3.5 seconds run time
  for (int k = 0; k < 300; k++) {
    // Filling up 512*2 samples at a time, making sure the sinus wave is unbroken
    // 200MHz in left channel, 2000MHz in right
    for (int n = 0; n < kBufferSize / 2; n++) {
      in[n * 2] = sin(2 * M_PI * 200 / 44100 * (n + ((float)k * kBufferSize / 2))) * 20000;
      in[n * 2 + 1] = sin(2 * M_PI * 2000 / 44100 * (n + ((float)k * kBufferSize / 2))) * 20000;
    }

    analyzer->Execute(in.data(), kBufferSize, out.data());
  }

  // Rounding last output to nearest 1/1000th
  for (int i = 0; i < out_size; i++) {
    out[i] = (double)round(out[i] * 1000) / 1000;
  }

  // Split result by channel
  std::vector<double> left(out.begin(), out.begin() + 10);
  std::vector<double> right(out.begin() + 10, out.begin() + 20);

  // Print results
  std::cout.setf(std::ios::fixed, std::ios::floatfield);
  std::cout << "\nlast output from channel left, max value should be at 200Hz:\n";
  for (const auto& value : left) {
    std::cout << std::setprecision(3) << value << " \t";
  }
  std::cout << "MHz\n\n";

  std::cout << "last output from channel right,  max value should be at 2000Hz:\n";
  for (const auto& value : right) {
    std::cout << std::setprecision(3) << value << " \t";
  }
  std::cout << "MHz\n\n";

  // Check that values are equal to expectation
  ASSERT_THAT(left, ElementsAreArray(expected_200MHz));
  ASSERT_THAT(right, ElementsAreArray(expected_2000MHz));
}

}  // namespace
