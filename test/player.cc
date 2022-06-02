#include "audio/player.h"

#include <gmock/gmock-matchers.h>   // for StrEq, EXPECT_THAT
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <chrono>
#include <iostream>
#include <thread>

#include "model/global_resource.h"

namespace {

/**
 * @brief Tests with Player class
 */
class PlayerTest : public ::testing::Test {
 protected:
  void SetUp() override { resources_ = std::make_shared<model::GlobalResource>(); }
  void TearDown() override { resources_.reset(); }

 protected:
  std::shared_ptr<model::GlobalResource> resources_;
};

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreateDummyPlayer) {
  auto player = audio::Player::Create(resources_);
  std::cout << "thread should not start playing yet" << std::endl;

  resources_->NotifyToExit();
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreatePlayerAndStartPlaying) {
  auto player = audio::Player::Create(resources_);
  std::cout << "thread should not start playing yet" << std::endl;

  resources_->play.store(true);
  std::cout << "alright, thread should have started now" << std::endl;

  resources_->NotifyToExit();
}

}  // namespace