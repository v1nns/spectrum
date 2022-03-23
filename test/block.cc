// #include "ui/base/block.h"

#include <memory>

#include "gtest/gtest.h"

namespace {

/* ********************************************************************************************** */

class BlockTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

 protected:
};

/* ********************************************************************************************** */

TEST_F(BlockTest, Dummy) {
  int value = 5 + 5;
  EXPECT_EQ(value, 10);
}

}  // namespace