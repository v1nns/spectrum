#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

#include "audio/driver/fftw.h"
#include "util/logger.h"

namespace {

using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Le;
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

  //! Print analysis result for each channel (left channel is the first half of bars)
  void PrintResults(const std::vector<double>& result) {
    const auto middle = result.begin() + kNumberBars;

    std::cout.setf(std::ios::fixed, std::ios::floatfield);
    std::cout << "\nlast output from channel left, max value should be at 200Hz:\n";
    for (auto it = result.begin(); it != middle; ++it) {
      std::cout << std::setprecision(3) << *it << " \t";
    }
    std::cout << "MHz\n\n";

    std::cout << "last output from channel right,  max value should be at 2000Hz:\n";
    for (auto it = middle; it != result.end(); ++it) {
      std::cout << std::setprecision(3) << *it << " \t";
    }
    std::cout << "MHz\n\n";
  }

  //! Execute analysis using a sinus wave (200Hz in left channel and 2000Hz in right channel)
  std::vector<double> Run(int frames) {
    std::vector<double> out(analyzer->GetOutputSize(), 0);
    std::vector<double> in(kBufferSize, 0);

    for (int k = 0; k < frames; k++, frame_++) {
      for (int n = 0; n < kBufferSize / 2; n++) {
        const double t = n + (static_cast<double>(frame_) * kBufferSize / 2);
        in[n * 2] = sin(2 * M_PI * 200 / 44100 * t) * 20000;
        in[(n * 2) + 1] = sin(2 * M_PI * 2000 / 44100 * t) * 20000;
      }

      analyzer->Execute(in.data(), kBufferSize, out.data());
    }

    return out;
  }

 protected:
  static constexpr int kNumberBars = 10;    //!< Number of bars per channel
  static constexpr int kBufferSize = 1024;  //!< Input buffer size

  Fftw analyzer;   //!< Audio frequency analysis
  int frame_ = 0;  //!< Frame counter, to keep sinus wave unbroken between calls to Run
};

/* ********************************************************************************************** */

TEST_F(FftwTest, InitAndExecute) {
  // Create expected results
  const Matcher<double> expected_200MHz[kNumberBars] = {0, 0, 0.999, 0.009, 0, 0.001, 0, 0, 0, 0};
  const Matcher<double> expected_2000MHz[kNumberBars] = {0, 0, 0, 0, 0, 0, 0.524, 0.474, 0, 0};

  // Running execute 300 times (simulating about 3.5 seconds run time)
  constexpr int kFrames = 300;
  auto out = Run(kFrames);

  // Rounding last output to nearest 1/1000th
  for (auto& value : out) {
    value = round(value * 1000) / 1000;
  }

  // Split result by channel
  std::vector<double> left(out.begin(), out.begin() + kNumberBars);
  std::vector<double> right(out.begin() + kNumberBars, out.end());

  PrintResults(out);

  // Check that values are equal to expectation
  ASSERT_THAT(left, ElementsAreArray(expected_200MHz));
  ASSERT_THAT(right, ElementsAreArray(expected_2000MHz));
}

/* ********************************************************************************************** */

TEST_F(FftwTest, SensitivityAdjustsQuickly) {
  // About 0.35 seconds of audio
  constexpr int kFrames = 30;
  const auto out = Run(kFrames);

  // Bar for 200Hz is already close to its final value, and no bar exceeds the maximum value
  EXPECT_GT(out[2], 0.8);
  EXPECT_THAT(out, Each(Le(1.0)));
}

}  // namespace
