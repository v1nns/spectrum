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
#include "sync_testing.h"

namespace {

using ::testing::_;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

using testing::SyncTesting;

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
    InSequence seq;

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

  bool HasExited() { return audio_player->exit_; }

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
  auto player = [&](SyncTesting& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"dummy"};

    // Setup all expectations
    InSequence seq;

    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([](int dummy, std::function<bool(void*, int, int)> callback) {
          callback(0, 0, 0);
          return false;
        }));

    EXPECT_CALL(*playback, AudioCallback(_, _, _));

    EXPECT_CALL(*notifier, ClearSongInformation()).WillOnce(Invoke([&] { syncer.NotifyStep(2); }));

    syncer.NotifyStep(1);
  };

  auto client = [&](SyncTesting& syncer) {
    syncer.WaitForStep(1);
    const std::string filename{"dummy"};

    // Ask Audio Player to play file
    audio_player->Play(filename);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(2);
    audio_player->Exit();
  };

  testing::RunSyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingAndPause) {
  auto player = [&](SyncTesting& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"Blinding Lights"};

    // Setup all expectations
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_name)));
    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again right after Pause was called
    EXPECT_CALL(*playback, Prepare()).Times(2).WillRepeatedly(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, std::function<bool(void*, int, int)> callback) {
          syncer.NotifyStep(2);
          callback(0, 0, 0);
          return false;
        }));

    EXPECT_CALL(*playback, Pause());

    EXPECT_CALL(*playback, AudioCallback(_, _, _));

    EXPECT_CALL(*notifier, ClearSongInformation()).WillOnce(Invoke([&] { syncer.NotifyStep(3); }));

    syncer.NotifyStep(1);
  };

  auto client = [&](SyncTesting& syncer) {
    syncer.WaitForStep(1);
    const std::string filename{"Blinding Lights"};

    // Ask Audio Player to play file and instantly pause it
    audio_player->Play(filename);
    audio_player->PauseOrResume();

    // Wait until Player starts decoding before client asks to resume
    syncer.WaitForStep(2);
    audio_player->PauseOrResume();

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(3);
    audio_player->Exit();
  };

  testing::RunSyncTest({player, client});
}

}  // namespace