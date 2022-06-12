#include "audio/player.h"

#include <gmock/gmock-matchers.h>   // for StrEq, EXPECT_THAT
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <chrono>
#include <memory>
#include <thread>

namespace {

/**
 * @brief Tests with Player class
 */
class PlayerTest : public ::testing::Test {
 protected:
  void SetUp() override { audio_player = audio::Player::Create(); }
  void TearDown() override { audio_player.reset(); }

 private:
  using Player = std::shared_ptr<audio::Player>;

 protected:
  Player audio_player;
};

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreateDummyPlayer) {
  //
  audio_player->Exit();
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreatePlayerAndStartPlaying) {
  audio_player->Play("dummy");

  audio_player->Exit();
}

}  // namespace