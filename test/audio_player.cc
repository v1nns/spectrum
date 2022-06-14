#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <chrono>
#include <memory>
#include <thread>

#include "audio/player.h"
#include "mock/decoder_mock.h"
#include "mock/playback_mock.h"
#include "model/application_error.h"

namespace {

using ::testing::_;
using ::testing::Field;

/**
 * @brief Tests with Player class
 */
class PlayerTest : public ::testing::Test {
  // using-declaration for Player
  using Player = std::shared_ptr<audio::Player>;

 protected:
  void SetUp() override {
    // Create mocks
    PlaybackMock* pb_mock = new PlaybackMock();
    DecoderMock* dc_mock = new DecoderMock();

    // Setup init expectations
    EXPECT_CALL(*pb_mock, CreatePlaybackStream());
    EXPECT_CALL(*pb_mock, ConfigureParameters());
    EXPECT_CALL(*pb_mock, GetPeriodSize());

    // Create Player
    audio_player = audio::Player::Create(pb_mock, dc_mock);
  }

  void TearDown() override { audio_player.reset(); }

  //! Getter for Playback
  auto GetPlayback() -> PlaybackMock* {
    return reinterpret_cast<PlaybackMock*>(audio_player->playback_.get());
  }

  //! Getter for Decoder
  auto GetDecoder() -> DecoderMock* {
    return reinterpret_cast<DecoderMock*>(audio_player->decoder_.get());
  }

 protected:
  Player audio_player;  //!< Audio player responsible for playing songs
};

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreateDummyPlayer) {
  // Dummy testing to check setup expectation, and then, exit
  audio_player->Exit();
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreatePlayerAndStartPlaying) {
  // Received filepath to play
  const std::string filename{"dummy"};

  auto playback = GetPlayback();
  auto decoder = GetDecoder();

  EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, filename)));
  EXPECT_CALL(*playback, Prepare());
  EXPECT_CALL(*decoder, Decode(_, _));
  // TODO: how to check if audio_player_->AudioCallback() was called...

  audio_player->Play(filename);

  // TODO: improve this
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(500ms);

  audio_player->Exit();
}

}  // namespace