#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <chrono>
#include <memory>
#include <thread>

#include "audio/player.h"
#include "general/sync_testing.h"
#include "mock/decoder_mock.h"
#include "mock/interface_notifier_mock.h"
#include "mock/playback_mock.h"
#include "model/application_error.h"
#include "util/logger.h"

namespace {

using ::testing::_;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

using testing::TestSyncer;

/**
 * @brief Tests with Player class
 */
class PlayerTest : public ::testing::Test {
  // using-declarations
  using Player = std::shared_ptr<audio::Player>;
  using NotifierMock = std::shared_ptr<InterfaceNotifierMock>;

 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override { Init(); }

  void TearDown() override {
    audio_player.reset();
    notifier.reset();
  }

  void Init(bool asynchronous = false) {
    // Create mocks
    PlaybackMock* pb_mock = new PlaybackMock();
    DecoderMock* dc_mock = new DecoderMock();

    // Setup init expectations
    InSequence seq;

    EXPECT_CALL(*pb_mock, CreatePlaybackStream());
    EXPECT_CALL(*pb_mock, ConfigureParameters());
    EXPECT_CALL(*pb_mock, GetPeriodSize());

    // Create Player without thread
    audio_player = audio::Player::Create(pb_mock, dc_mock, asynchronous);

    // Register interface notifier to Audio Player
    notifier = std::make_shared<InterfaceNotifierMock>();
    audio_player->RegisterInterfaceNotifier(notifier);
  }

  //! Getter for Playback (necessary as inner variable is an unique_ptr)
  auto GetPlayback() -> PlaybackMock* {
    return reinterpret_cast<PlaybackMock*>(audio_player->playback_.get());
  }

  //! Getter for Decoder (necessary as inner variable is an unique_ptr)
  auto GetDecoder() -> DecoderMock* {
    return reinterpret_cast<DecoderMock*>(audio_player->decoder_.get());
  }

  //! Getter for Public API for Player media control
  auto GetAudioControl() -> std::shared_ptr<audio::AudioControl> { return audio_player; }

  //! Run audio loop (same one executed as a thread in the real-life)
  void RunAudioLoop() { audio_player->AudioHandler(); }

 protected:
  Player audio_player;    //!< Audio player responsible for playing songs
  NotifierMock notifier;  //!< API for audio player to send interface events
};

/* ********************************************************************************************** */

class PlayerTestThread : public PlayerTest {
 protected:
  void SetUp() override { Init(true); }
};

TEST_F(PlayerTestThread, CreateDummyPlayer) {
  // Dummy testing to check setup expectation, and then, exit
  audio_player->Exit();
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreatePlayerAndStartPlaying) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"The Police - Roxanne"};

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
        .WillOnce(Invoke([](int dummy, driver::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, position);
          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _));
    EXPECT_CALL(*playback, AudioCallback(_, _));

    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Play, .position = 0}));
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    const std::string filename{"The Police - Roxanne"};

    // Ask Audio Player to play file
    player_ctl->Play(filename);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingAndPause) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"The Weeknd - Blinding Lights"};

    // Setup all expectations
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again right after Pause was called
    EXPECT_CALL(*playback, Prepare()).Times(2).WillRepeatedly(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
          // Starts playing
          int64_t position = 0;
          callback(0, 0, position);

          // Notify other thread to ask for pause and wait for it
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Pause and wait to resume
          position++;
          callback(0, 0, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*playback, Pause());

    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(2);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(2);

    // Using-declaration to improve readability
    using State = model::Song::MediaState;

    EXPECT_CALL(*notifier,
                NotifySongState(Field(&model::Song::CurrentInformation::state, State::Play)))
        .Times(2);

    EXPECT_CALL(*notifier,
                NotifySongState(Field(&model::Song::CurrentInformation::state, State::Pause)))
        .WillOnce(Invoke([&] { syncer.NotifyStep(4); }));

    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(5);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    const std::string filename{"The Weeknd - Blinding Lights"};

    // Ask Audio Player to play file and instantly pause it
    player_ctl->Play(filename);

    // Wait until Player starts decoding before client asks to pause
    syncer.WaitForStep(2);
    player_ctl->PauseOrResume();
    syncer.NotifyStep(3);

    // Resume after player is paused
    syncer.WaitForStep(4);
    player_ctl->PauseOrResume();

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(5);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingAndStop) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"RÜFÜS - Innerbloom (What So Not Remix)"};

    // Setup all expectations
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Invoke([&] {
          // Notify step here to give enough time for client to ask for stop
          syncer.NotifyStep(2);
          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again right after Stop was called
    EXPECT_CALL(*playback, Prepare());

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself so it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
          syncer.WaitForStep(3);

          int64_t position = 0;
          callback(0, 0, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);
    EXPECT_CALL(*playback, Stop());

    EXPECT_CALL(*notifier, NotifySongState(_)).Times(AtMost(1));
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(4);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play file
    const std::string filename{"RÜFÜS - Innerbloom (What So Not Remix)"};
    player_ctl->Play(filename);

    // Wait for Player to prepare for playing
    syncer.WaitForStep(2);
    player_ctl->Stop();

    // Notify audio player to execute Decode callback right after the Stop command is sent
    syncer.NotifyStep(3);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(4);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingAndUpdateSongState) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"The White Stripes - Blue Orchid"};

    // Setup all expectations
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again right after Pause was called
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
          int64_t position = 1;
          callback(0, 0, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _));
    EXPECT_CALL(*playback, AudioCallback(_, _));

    // In this case, decoder will tell us that the current timestamp matches some position other
    // than zero (this value is represented in seconds). And for this, we should notify Media Player
    // to update its graphical interface
    uint32_t expected_position = 1;
    EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::position,
                                                 expected_position)));

    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    const std::string filename{"The White Stripes - Blue Orchid"};

    // Ask Audio Player to play file
    player_ctl->Play(filename);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorOpeningFile) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"Cannons - Round and Round"};

    // Setup all expectations
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kFileNotSupported));

    // None of these should be called in this situation
    EXPECT_CALL(*notifier, NotifySongInformation(_)).Times(0);
    EXPECT_CALL(*playback, Prepare()).Times(0);
    EXPECT_CALL(*decoder, Decode(_, _)).Times(0);
    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

    // Only these should be called
    EXPECT_CALL(*notifier, ClearSongInformation(false));
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kFileNotSupported))).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    const std::string filename{"Cannons - Round and Round"};

    // Ask Audio Player to play file
    player_ctl->Play(filename);

    // Wait for Player to notify error before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorDecodingFile) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"Yung Buda - Sozinho no Tougue"};

    // Setup all expectations
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*decoder, Decode(_, _)).WillOnce(Return(error::kUnknownError));

    // This should not be called in this situation
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

    // Only these should be called
    EXPECT_CALL(*notifier, ClearSongInformation(true));
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kUnknownError))).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    const std::string filename{"Yung Buda - Sozinho no Tougue"};

    // Ask Audio Player to play file
    player_ctl->Play(filename);

    // Wait for Player to notify error before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ChangeVolume) {
  auto decoder = GetDecoder();
  auto player_ctl = GetAudioControl();

  // As decoder is just an interface, use this variable to hold volume information and setup
  // expectation for decoder from player to always return the same variable
  model::Volume value;
  EXPECT_CALL(*decoder, GetVolume()).WillRepeatedly(Invoke([&] { return value; }));

  // Setup expectation for default value on volume
  EXPECT_THAT(player_ctl->GetAudioVolume(), Eq(model::Volume{1.f}));

  // Setup expectation for decoder and set new volume on player
  EXPECT_CALL(*decoder, SetVolume(_)).WillOnce(Invoke([&](model::Volume other) {
    value = other;
    return error::kSuccess;
  }));

  player_ctl->SetAudioVolume({0.3f});

  // Get updated volume from player
  EXPECT_THAT(player_ctl->GetAudioVolume(), Eq(model::Volume{0.3f}));

  // TODO: return error::Code on player API and create a test forcing error on volume change
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingSeekForwardAndBackward) {
  const std::string song{"Mareux - Summertime"};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Setup all expectations
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, song)))
        .WillOnce(Invoke([&](model::Song& audio_info) {
          // To enable seek position feature, must fill duration info to song struct
          audio_info.duration = 15;
          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again right after Pause was called
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
          int64_t position = 0;
          syncer.NotifyStep(2);
          callback(0, 0, position);
          syncer.WaitForStep(3);

          for (int i = 0; i <= 3; i++) {
            position++;
            callback(0, 0, position);
          }

          // This value is considering the seek backward/forward commands + sum in the for-loop
          EXPECT_EQ(5, position);

          return error::kSuccess;
        }));

    // These methods should be called only one time because of seek backward/forward command
    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(2);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(2);
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(2);

    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(4);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play file
    player_ctl->Play(song);

    // Ask Audio Player to seek forward position in song by 1 second
    syncer.WaitForStep(2);
    player_ctl->SeekForwardPosition(1);
    player_ctl->SeekBackwardPosition(1);
    player_ctl->SeekForwardPosition(1);
    syncer.NotifyStep(3);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(4);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, TryToSeekWhilePaused) {
  const std::string song{"Joji - Glimpse of Us"};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Setup all expectations
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, song)))
        .WillOnce(Invoke([&](model::Song& audio_info) {
          // To enable seek position feature, must fill duration info to song struct
          audio_info.duration = 15;
          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again right after Pause was called
    EXPECT_CALL(*playback, Prepare()).Times(2).WillRepeatedly(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          for (int i = 0; i <= 3; i++) {
            position++;
            callback(0, 0, position);
          }

          // This value is considering the seek backward/forward commands + sum in the for-loop
          EXPECT_EQ(4, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*playback, Pause());

    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(5);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(5);

    // Using-declaration to improve readability
    using State = model::Song::MediaState;

    // This is called 5 times because of position update notification
    EXPECT_CALL(*notifier,
                NotifySongState(Field(&model::Song::CurrentInformation::state, State::Play)))
        .Times(5);

    EXPECT_CALL(*notifier,
                NotifySongState(Field(&model::Song::CurrentInformation::state, State::Pause)))
        .WillOnce(Invoke([&] { syncer.NotifyStep(4); }));

    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(5);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play file
    player_ctl->Play(song);

    // Wait until Player starts decoding to pause
    syncer.WaitForStep(2);
    player_ctl->PauseOrResume();
    syncer.NotifyStep(3);

    syncer.WaitForStep(4);
    player_ctl->SeekForwardPosition(1);
    player_ctl->SeekForwardPosition(1);
    player_ctl->SeekForwardPosition(1);

    // Wait until Player pauses, to resume song
    player_ctl->PauseOrResume();

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(5);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingAndRequestNewSong) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepaths to play
    const std::string expected_filename1{"Stephen - I Never Stay in Love"};
    const std::string expected_filename2{"Lorn - Acid Rain"};

    /* ****************************************************************************************** */
    // Setup expectation for first song
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_filename1)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called before start playing
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself. So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
          int64_t position = 1;
          callback(0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          position++;
          callback(0, 0, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _));
    EXPECT_CALL(*playback, AudioCallback(_, _));

    EXPECT_CALL(*playback, Stop());

    // In this case, decoder will tell us that the current timestamp matches some position other
    // than zero (this value is represented in seconds). And for this, we should notify Media
    // Player to update its graphical interface
    uint32_t expected_position = 1;
    EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::position,
                                                 expected_position)));

    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      /* ************************************************************************************** */
      // ATTENTION: this is the workaround found to iterate in a new audio loop to play (using the
      // invoke function to call it from the previous loop)

      // Setup expectation for second song
      EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_filename2)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(_));

      // Prepare is called before start playing
      EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

      // In this case, this won't even play at all, it will wait for the Exit command from client
      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
            int64_t position = 0;

            syncer.NotifyStep(4);
            syncer.WaitForStep(5);

            callback(0, 0, position);
            return error::kSuccess;
          }));

      EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
      EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

      expected_position = 0;
      EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::position,
                                                   expected_position)))
          .Times(0);

      EXPECT_CALL(*notifier, ClearSongInformation(true));
    }));

    /* ****************************************************************************************** */
    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    const std::string filename1{"Stephen - I Never Stay in Love"};
    const std::string filename2{"Lorn - Acid Rain"};

    // Ask Audio Player to play file
    player_ctl->Play(filename1);

    // Wait until Player starts decoding to send a new song request
    syncer.WaitForStep(2);
    player_ctl->Play(filename2);
    syncer.NotifyStep(3);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(4);
    player_ctl->Exit();
    syncer.NotifyStep(5);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingThenPauseAndRequestNewSong) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepaths to play
    const std::string expected_filename1{"Ookay - Thief"};
    const std::string expected_filename2{"Tame Impala - Elephant"};

    /* ****************************************************************************************** */
    // Setup expectation for first song
    EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_filename1)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare() should be called only once, because when we receive a new Play command,
    // it should exit from loop
    EXPECT_CALL(*playback, Prepare()).Times(1).WillRepeatedly(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself. So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
          int64_t position = 1;
          callback(0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // This next callback call will be blocked until receives some of the expected commands
          // for Paused state
          position++;
          callback(0, 0, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*playback, Pause());

    EXPECT_CALL(*notifier, SendAudioRaw(_, _));
    EXPECT_CALL(*playback, AudioCallback(_, _));

    EXPECT_CALL(*playback, Stop());

    // In this case, decoder will tell us that the current timestamp matches some position other
    // than zero (this value is represented in seconds). And for this, we should notify Media
    // Player to update its graphical interface
    uint32_t expected_position = 1;
    EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::position,
                                                 expected_position)));

    EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::state,
                                                 model::Song::MediaState::Pause)));

    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      /* ************************************************************************************** */
      // ATTENTION: this is the workaround found to iterate in a new audio loop to play (using the
      // invoke function to call it from the previous loop)

      // Setup expectation for second song
      EXPECT_CALL(*decoder, OpenFile(Field(&model::Song::filepath, expected_filename2)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(_));

      // Prepare is called before start playing
      EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

      // In this case, this won't even play at all, it will wait for the Exit command from client
      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([&](int dummy, driver::Decoder::AudioCallback callback) {
            int64_t position = 0;

            syncer.NotifyStep(4);
            syncer.WaitForStep(5);

            callback(0, 0, position);
            return error::kSuccess;
          }));

      EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
      EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

      expected_position = 0;
      EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::position,
                                                   expected_position)))
          .Times(0);

      EXPECT_CALL(*notifier, ClearSongInformation(true));
    }));

    /* ****************************************************************************************** */
    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    const std::string filename1{"Ookay - Thief"};
    const std::string filename2{"Tame Impala - Elephant"};

    // Ask Audio Player to play file
    player_ctl->Play(filename1);

    // Wait until Player starts decoding to pause
    syncer.WaitForStep(2);
    player_ctl->PauseOrResume();
    syncer.NotifyStep(3);

    // Wait a bit, just until Player pauses
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send any command, just to check that it will be ignored by audio thread
    player_ctl->SeekForwardPosition(1);
    player_ctl->SeekBackwardPosition(1);
    player_ctl->SetAudioVolume({0.5f});

    // Now send a new song request
    player_ctl->Play(filename2);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(4);
    player_ctl->Exit();
    syncer.NotifyStep(5);
  };

  testing::RunAsyncTest({player, client});
}

}  // namespace
