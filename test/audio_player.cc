#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <chrono>
#include <memory>
#include <thread>

#include "audio/player.h"
#include "mock/decoder_mock.h"
#include "mock/interface_notifier_mock.h"
#include "mock/playback_mock.h"
#include "model/application_error.h"

namespace {

using ::testing::_;
using ::testing::Field;
using ::testing::Invoke;

/**
 * @brief Tests with Player class
 */
class PlayerTest : public ::testing::Test {
  // using-declarations
  using Player = std::shared_ptr<audio::Player>;
  using NotifierMock = std::shared_ptr<InterfaceNotifierMock>;

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

    // Register interface notifier to Audio Player
    notifier = std::make_shared<InterfaceNotifierMock>();
    audio_player->RegisterInterfaceNotifier(notifier);
  }

  void TearDown() override {
    audio_player.reset();
    notifier.reset();
  }

  //! Getter for Playback
  auto GetPlayback() -> PlaybackMock* {
    return reinterpret_cast<PlaybackMock*>(audio_player->playback_.get());
  }

  //! Getter for Decoder
  auto GetDecoder() -> DecoderMock* {
    return reinterpret_cast<DecoderMock*>(audio_player->decoder_.get());
  }

 protected:
  Player audio_player;    //!< Audio player responsible for playing songs
  NotifierMock notifier;  //!< API for audio player to send interface events
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

  // Setup all expectations
  EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, filename)));
  EXPECT_CALL(*notifier, NotifySongInformation(_));

  EXPECT_CALL(*playback, Prepare());

  EXPECT_CALL(*playback, AudioCallback(_, _, _));

  // Only interested in second argument, which is a lambda created internally by audio_player itself
  // So it is necessary to manually call it, to keep the behaviour similar to a real-life situation
  EXPECT_CALL(*decoder, Decode(_, _))
      .WillOnce(Invoke([](int dummy, std::function<bool(void*, int, int)> callback) {
        callback(0, 0, 0);
        return error::kSuccess;
      }));

  EXPECT_CALL(*notifier, ClearSongInformation());

  // Ask Audio Player to play file
  audio_player->Play(filename);

  // TODO: improve this attempt to exit from thread
  // It is bad, I know...
  using namespace std::chrono_literals;
  std::this_thread::sleep_for(100ms);

  audio_player->Exit();
}

}  // namespace