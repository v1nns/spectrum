#include "audio/player.h"

#include <gmock/gmock-matchers.h>   // for StrEq, EXPECT_THAT
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <chrono>
#include <iostream>
#include <thread>

namespace {

/**
 * @brief Tests with Player class
 */
class PlayerTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreateDummyPlayer) {
  auto player = audio::Player::Create();
  std::cout << "thread should not start playing yet" << std::endl;

  player->Exit();
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreatePlayerAndStartPlaying) {
  auto player = audio::Player::Create();
  std::cout << "thread should not start playing yet" << std::endl;

  player->Play("dummy");
  std::cout << "alright, thread should have started now" << std::endl;

  player->Exit();
}

}  // namespace