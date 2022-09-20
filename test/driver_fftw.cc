#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <cmath>
#include <memory>

#include "audio/driver/fftw.h"
#include "model/application_error.h"

namespace {

using ::testing::_;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

/**
 * @brief Tests with FFTW class
 */
class FftwTest : public ::testing::Test {
  // using-declarations
  using Fftw = std::unique_ptr<driver::FFTW>;

 protected:
  void SetUp() override { Init(); }

  void TearDown() override { analyzer.reset(); }

  void Init() {
    analyzer = std::make_unique<driver::FFTW>();
    analyzer->Init();
  }

 protected:
  Fftw analyzer;  //!< TODO:
};

/* ********************************************************************************************** */

TEST_F(FftwTest, InitAndExecute) {
  printf("allocating buffers and generating sine wave for test\n\n");

  double blueprint_2000MHz[10] = {0, 0, 0, 0, 0, 0, 0.524, 0.474, 0, 0};
  double blueprint_200MHz[10] = {0, 0, 0.998, 0.009, 0, 0.001, 0, 0, 0, 0};

  double *cava_out;
  double *cava_in;

  cava_out = (double *)malloc(10 * 2 * sizeof(double));
  cava_in = (double *)malloc(1024 * sizeof(double));

  for (int i = 0; i < 10 * 2; i++) {
    cava_out[i] = 0;
  }

  printf("running cava execute 300 times (simulating about 3.5 seconds run time)\n\n");
  for (int k = 0; k < 300; k++) {
    // filling up 512*2 samples at a time, making sure the sinus wave is unbroken
    // 200MHz in left channel, 2000MHz in right
    // if we where using a proper audio source this would be replaced by a simple read function
    for (int n = 0; n < 1024 / 2; n++) {
      cava_in[n * 2] = sin(2 * M_PI * 200 / 44100 * (n + (k * 1024 / 2))) * 20000;
      cava_in[n * 2 + 1] = sin(2 * M_PI * 2000 / 44100 * (n + (k * 1024 / 2))) * 20000;
    }

    analyzer->Execute(cava_in, 1024, cava_out);
  }

  // rounding last output to nearst 1/1000th
  for (int i = 0; i < 10 * 2; i++) {
    cava_out[i] = (double)round(cava_out[i] * 1000) / 1000;
  }

  printf("\nlast output left, max value should be at 200Hz:\n");
  for (int i = 0; i < 10; i++) {
    printf("%.3f \t", cava_out[i]);
  }
  printf("MHz\n");

  printf("last output right,  max value should be at 2000Hz:\n");
  for (int i = 0; i < 10; i++) {
    printf("%.3f \t", cava_out[i + 10]);
  }
  printf("MHz\n\n");

  // checking if within 2% of blueprint
  int bp_ok = 1;
  for (int i = 0; i < 10; i++) {
    if (cava_out[i] > blueprint_200MHz[i] * 1.02 || cava_out[i] < blueprint_200MHz[i] * 0.98)
      bp_ok = 0;
  }
  for (int i = 0; i < 10; i++) {
    if (cava_out[i + 10] > blueprint_2000MHz[i] * 1.02 ||
        cava_out[i + 10] < blueprint_2000MHz[i] * 0.98)
      bp_ok = 0;
  }

  free(cava_in);
  free(cava_out);
  if (bp_ok == 1) {
    printf("matching blueprint\n");
    exit(0);
  } else {
    printf("not matching blueprint\n");
    exit(1);
  }
}

}  // namespace
