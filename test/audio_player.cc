#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "audio/command.h"
#include "audio/player.h"
#include "general/sync_testing.h"
#include "mock/decoder_mock.h"
#include "mock/interface_notifier_mock.h"
#include "mock/playback_mock.h"
#include "mock/stream_fetcher_mock.h"
#include "model/application_error.h"
#include "model/playlist.h"
#include "model/stream_info.h"
#include "util/logger.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;

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

  //! Create player, optionally with an output device chosen by user (which may not be available)
  void Init(bool asynchronous = false, const std::string& device = "",
            bool device_available = true) {
    // Create mocks
    PlaybackMock* pb_mock = new PlaybackMock();
    DecoderMock* dc_mock = new DecoderMock();
    StreamFetcherMock* sf_mock = new StreamFetcherMock();

    notifier = std::make_shared<InterfaceNotifierMock>();

    // Format is exchanged between playback and decoder (by default, playback expects the same
    // format that is asked by player)
    EXPECT_CALL(*pb_mock, GetFormat()).Times(AnyNumber());
    EXPECT_CALL(*dc_mock, SetOutputFormat(_)).Times(AnyNumber());

    // Setup init expectations
    InSequence seq;

    if (!device_available) {
      // Playback chooses the device when the one chosen by user cannot be used
      EXPECT_CALL(*pb_mock, CreatePlaybackStream(device))
          .WillOnce(Return(error::kOpenDeviceFailed));
      EXPECT_CALL(*pb_mock, CreatePlaybackStream(""));
    } else {
      EXPECT_CALL(*pb_mock, CreatePlaybackStream(device));
    }

    EXPECT_CALL(*pb_mock, ConfigureParameters(_));
    EXPECT_CALL(*pb_mock, GetPeriodSize());

    // And interface is notified about it as soon as it is registered
    if (!device_available) {
      EXPECT_CALL(*notifier, NotifyError(Eq(error::kOpenDeviceFailed), StrEq(device)));
    }

    // Create Player without thread
    audio_player =
        audio::Player::Create(/*verbose=*/true, device, pb_mock, dc_mock, sf_mock, asynchronous);

    // Register interface notifier to Audio Player
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

  //! Getter for StreamFetcher (necessary as inner variable is an unique_ptr)
  auto GetStreamFetcher() -> StreamFetcherMock* {
    return reinterpret_cast<StreamFetcherMock*>(audio_player->fetcher_.get());
  }

  //! Getter for Public API for Player media control
  auto GetAudioControl() -> std::shared_ptr<audio::AudioControl> { return audio_player; }

  //! Run audio loop (same one executed as a thread in the real-life)
  void RunAudioLoop() { audio_player->AudioHandler(); }

  //! Check if player still has a playlist to play songs from
  bool HasPlaylist() const { return audio_player->curr_playlist_.has_value(); }

  //! Start playing (single file or playlist) and check that skip commands are ignored, as there is
  //! no song to skip to (song keeps playing until client asks to exit)
  void CheckSkipIsIgnored(const std::function<void(audio::AudioControl&)>& play,
                          const std::string& filepath, const std::vector<bool>& skip_to_next) {
    auto player = [&](TestSyncer& syncer) {
      auto decoder = GetDecoder();

      EXPECT_CALL(*GetPlayback(), Prepare()).WillOnce(Return(error::kSuccess));
      EXPECT_CALL(*notifier, NotifySongState(_)).Times(AnyNumber());

      // Song is opened only once
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, filepath)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;
            callback(0, 0, 0, 0, position);

            syncer.NotifyStep(2);
            syncer.WaitForStep(3);

            // Each skip command is ignored, so song keeps playing
            for (size_t i = 0; i < skip_to_next.size(); ++i) {
              EXPECT_TRUE(callback(0, 0, 0, 0, position));
            }

            syncer.NotifyStep(4);
            syncer.WaitForStep(5);

            // Exit
            EXPECT_FALSE(callback(0, 0, 0, 0, position));
            return error::kSuccess;
          }));

      // Notify that expectations are set, and run audio loop
      syncer.NotifyStep(1);
      RunAudioLoop();
    };

    auto client = [&](TestSyncer& syncer) {
      auto player_ctl = GetAudioControl();
      syncer.WaitForStep(1);
      play(*player_ctl);

      syncer.WaitForStep(2);
      for (bool next : skip_to_next) next ? player_ctl->SkipToNext() : player_ctl->SkipToPrevious();
      syncer.NotifyStep(3);

      syncer.WaitForStep(4);
      player_ctl->Exit();
      syncer.NotifyStep(5);
    };

    testing::RunAsyncTest({player, client});
  }

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

    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    // Decoded audio contains 16-bit samples with interleaved channels (stereo)
    constexpr int kFrames = 4;
    constexpr int kChannels = 2;

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
          std::vector<int16_t> samples(kFrames * kChannels, 0);
          int64_t position = 0;
          callback(samples.data(), kFrames, nullptr, 0, position);
          return error::kSuccess;
        }));

    // Audio analysis receives all samples (from both channels), while playback receives frames
    EXPECT_CALL(*notifier, SendAudioRaw(_, kFrames * kChannels));
    EXPECT_CALL(*playback, AudioCallback(_, kFrames));

    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Play, .position = 0}));

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}));
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
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again right after Pause was called
    EXPECT_CALL(*playback, Prepare()).Times(2).WillRepeatedly(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          // Starts playing
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          // Notify other thread to ask for pause and wait for it
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Pause and wait to resume
          position++;
          callback(0, 0, 0, 0, position);

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

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}));
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
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_name))).WillOnce(Invoke([&] {
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
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          syncer.WaitForStep(3);

          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);
    EXPECT_CALL(*playback, Stop());

    EXPECT_CALL(*notifier, NotifySongState(_)).Times(AtMost(1));
    EXPECT_CALL(*decoder, ClearCache());
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
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again right after Pause was called
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 1;
          callback(0, 0, 0, 0, position);

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

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}));
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
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kFileNotSupported));

    // None of these should be called in this situation
    EXPECT_CALL(*notifier, NotifySongInformation(_)).Times(0);
    EXPECT_CALL(*playback, Prepare()).Times(0);
    EXPECT_CALL(*decoder, Decode(_, _)).Times(0);
    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

    // Only these should be called
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier,
                NotifyError(Eq(error::kFileNotSupported), StrEq("Cannons - Round and Round")))
        .WillOnce(Invoke([&] { syncer.NotifyStep(2); }));
    EXPECT_CALL(*notifier, ClearSongInformation(false)).Times(0);

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
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*decoder, Decode(_, _)).WillOnce(Return(error::kUnknownError));

    // This should not be called in this situation
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

    // Only these should be called
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, ClearSongInformation(true));
    EXPECT_CALL(*notifier,
                NotifyError(Eq(error::kUnknownError), StrEq("Yung Buda - Sozinho no Tougue")))
        .WillOnce(Invoke([&] { syncer.NotifyStep(2); }));

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

TEST_F(PlayerTest, ErrorWritingToPlayback) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepath to play
    const std::string expected_name{"Daft Punk - Around the World"};

    // Setup all expectations
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_name)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Output device fails (e.g. it was disconnected), so decoding must stop
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
          std::vector<int16_t> samples(8, 0);
          int64_t position = 0;
          EXPECT_FALSE(callback(samples.data(), 4, nullptr, 0, position));
          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _));
    EXPECT_CALL(*playback, AudioCallback(_, _)).WillOnce(Return(error::kPlaybackFailed));

    // Song did not finish, it failed
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(0);

    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, ClearSongInformation(true));
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kPlaybackFailed), StrEq(expected_name)))
        .WillOnce(Invoke([&] { syncer.NotifyStep(2); }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play file
    player_ctl->Play(std::string{"Daft Punk - Around the World"});

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

  player_ctl->SetAudioVolume(model::Volume{0.3f});

  // Get updated volume from player
  EXPECT_THAT(player_ctl->GetAudioVolume(), Eq(model::Volume{0.3f}));

  // TODO: return error::Code on player API and create a test forcing error on volume change
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, GetAudioDevices) {
  const model::AudioDevices devices{
      {.name = "default", .description = "Default output"},
      {.name = "front:CARD=DAC,DEV=0", .description = "USB Audio"},
  };

  EXPECT_CALL(*GetPlayback(), ListDevices()).WillOnce(Return(devices));
  EXPECT_THAT(GetAudioControl()->GetAudioDevices(), Eq(devices));
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreatePlayerWithDevice) {
  // Device chosen by user is the only one used to create playback stream
  Init(/*asynchronous=*/false, "front:CARD=DAC,DEV=0");
  ::testing::Mock::VerifyAndClearExpectations(GetPlayback());
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, CreatePlayerWithUnavailableDevice) {
  Init(/*asynchronous=*/false, "front:CARD=DAC,DEV=0", /*device_available=*/false);
  ::testing::Mock::VerifyAndClearExpectations(GetPlayback());
  ::testing::Mock::VerifyAndClearExpectations(notifier.get());

  // Error is notified only once
  auto other = std::make_shared<InterfaceNotifierMock>();
  EXPECT_CALL(*other, NotifyError(_, _)).Times(0);
  audio_player->RegisterInterfaceNotifier(other);
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ChangeDeviceWithoutPlaying) {
  const std::string device{"front:CARD=DAC,DEV=0"};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();

    // New playback stream is created and configured on the chosen device
    InSequence seq;
    EXPECT_CALL(*playback, CreatePlaybackStream(device)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, ConfigureParameters(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, GetPeriodSize()).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
      return 0;
    }));

    EXPECT_CALL(*notifier, NotifyError(_, _)).Times(0);

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    player_ctl->SetAudioDevice(device);

    // Wait for Player to change device before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorChangingDeviceKeepsPreviousOne) {
  const std::string device{"front:CARD=DAC,DEV=0"};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();

    // New device does not support the parameters, so the previous one (chosen by playback, as
    // nothing was chosen by user until now) is used again
    InSequence seq;
    EXPECT_CALL(*playback, CreatePlaybackStream(device)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, ConfigureParameters(_)).WillOnce(Return(error::kUnknownError));
    EXPECT_CALL(*playback, CreatePlaybackStream("")).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, ConfigureParameters(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, GetPeriodSize());

    EXPECT_CALL(*notifier, NotifyError(Eq(error::kOpenDeviceFailed), StrEq(device)))
        .WillOnce(Invoke([&] { syncer.NotifyStep(2); }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    player_ctl->SetAudioDevice(device);

    // Wait for Player to notify error before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingAndChangeDevice) {
  const std::string device{"front:CARD=DAC,DEV=0"};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Setup all expectations
    EXPECT_CALL(*decoder, Open(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          // Starts playing
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          // Notify other thread to ask to change device and wait for it
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Device is changed and song keeps playing
          position++;
          EXPECT_TRUE(callback(0, 0, 0, 0, position));

          return error::kSuccess;
        }));

    // New playback stream is created without stopping the song
    EXPECT_CALL(*playback, CreatePlaybackStream(device)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, ConfigureParameters(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, GetPeriodSize());
    EXPECT_CALL(*playback, Stop()).Times(0);

    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(2);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(2);
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(3);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
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

    player_ctl->Play(std::string{"The Weeknd - Blinding Lights"});

    // Wait until Player starts decoding before client asks to change device
    syncer.WaitForStep(2);
    player_ctl->SetAudioDevice(device);
    syncer.NotifyStep(3);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(4);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ChangeDeviceToAnotherFormatAndStartPlaying) {
  const std::string device{"sysdefault:CARD=DAC"};
  const model::AudioFormat desired{.sample_format = model::SampleFormat::S32};
  const model::AudioFormat format{.sample_rate = 48000};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // New device does not support the desired format (widest sample format, with the default sample
    // rate while no song is played), so it expects another one
    EXPECT_CALL(*playback, CreatePlaybackStream(device)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, ConfigureParameters(Eq(desired))).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, GetFormat()).WillRepeatedly(Return(format));
    EXPECT_CALL(*playback, GetPeriodSize()).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
      return 0;
    }));

    // And decoder is asked to create samples in this format
    EXPECT_CALL(*decoder, Open(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, SetOutputFormat(Eq(format))).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Interface is notified about audio output only when song starts playing (not when device is
    // changed without any song)
    EXPECT_CALL(*playback, GetDevice()).WillOnce(Return(device));
    EXPECT_CALL(*notifier,
                NotifyAudioOutput(Eq(model::AudioOutput{.device = device, .format = format})));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    constexpr int kFrames = 4;
    constexpr int kAnalysisFrames = 3;
    constexpr int kChannels = 2;

    std::vector<int16_t> samples(kFrames * kChannels, 0);
    std::vector<int16_t> analysis(kAnalysisFrames * kChannels, 0);

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;

          // Samples to analysis have their own size, as their format is not the same
          callback(samples.data(), kFrames, analysis.data(), kAnalysisFrames, position);

          // Without them, samples to playback cannot be used by analysis
          callback(samples.data(), kFrames, nullptr, 0, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(analysis.data(), kAnalysisFrames * kChannels));
    EXPECT_CALL(*playback, AudioCallback(samples.data(), kFrames)).Times(2);
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(2);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(3);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    player_ctl->SetAudioDevice(device);

    // Wait for Player to change device before client asks to play
    syncer.WaitForStep(2);
    player_ctl->Play(std::string{"The Weeknd - Blinding Lights"});

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(3);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingAndChangeDeviceToAnotherFormat) {
  const std::string device{"sysdefault:CARD=DAC"};
  const model::AudioFormat format{.sample_rate = 96000, .sample_format = model::SampleFormat::S32};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Song starts with default format
    EXPECT_CALL(*decoder, Open(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, SetOutputFormat(Eq(model::AudioFormat{})))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    constexpr int kChannels = 2;
    std::vector<int16_t> first(4 * kChannels, 0);
    std::vector<int16_t> second(4 * kChannels, 0);
    std::vector<int32_t> third(4 * kChannels, 0);

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(first.data(), 4, nullptr, 0, position);

          // Notify other thread to ask to change device and wait for it
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // These samples are still in the previous format, so they are not sent to new device
          EXPECT_TRUE(callback(second.data(), 4, nullptr, 0, position));

          // Next ones are already in the format asked to decoder
          EXPECT_TRUE(callback(third.data(), 4, nullptr, 0, position));

          return error::kSuccess;
        }));

    // New device expects another format, so decoder is asked to change it
    EXPECT_CALL(*playback, CreatePlaybackStream(device)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, ConfigureParameters(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, GetFormat()).WillOnce(Return(format));
    EXPECT_CALL(*playback, GetPeriodSize());
    EXPECT_CALL(*decoder, SetOutputFormat(Eq(format))).WillOnce(Return(error::kSuccess));

    // Interface is notified about audio output when song starts playing, and again when device is
    // changed
    const std::string first_device{"default"};
    EXPECT_CALL(*playback, GetDevice()).WillOnce(Return(first_device)).WillOnce(Return(device));
    EXPECT_CALL(*notifier, NotifyAudioOutput(Eq(model::AudioOutput{.device = first_device})));
    EXPECT_CALL(*notifier,
                NotifyAudioOutput(Eq(model::AudioOutput{.device = device, .format = format})));

    EXPECT_CALL(*playback, AudioCallback(first.data(), 4));
    EXPECT_CALL(*playback, AudioCallback(second.data(), 4)).Times(0);
    EXPECT_CALL(*playback, AudioCallback(third.data(), 4));

    // Samples sent to playback are used by analysis only while they are in the format expected by
    // it (which is the default one)
    EXPECT_CALL(*notifier, SendAudioRaw(first.data(), 4 * kChannels));
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(2);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
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

    player_ctl->Play(std::string{"The Weeknd - Blinding Lights"});

    // Wait until Player starts decoding before client asks to change device
    syncer.WaitForStep(2);
    player_ctl->SetAudioDevice(device);
    syncer.NotifyStep(3);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(4);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorSettingOutputFormatOnDecoder) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Song is not played when decoder cannot create samples in the format expected by playback
    EXPECT_CALL(*decoder, Open(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, SetOutputFormat(_)).WillOnce(Return(error::kUnknownError));
    EXPECT_CALL(*decoder, Decode(_, _)).Times(0);
    EXPECT_CALL(*playback, Prepare()).Times(0);
    EXPECT_CALL(*notifier, NotifySongInformation(_)).Times(0);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kUnknownError), _)).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    player_ctl->Play(std::string{"The Weeknd - Blinding Lights"});

    // Wait for Player to notify error before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingThenPauseAndChangeDevice) {
  const std::string device{"front:CARD=DAC,DEV=0"};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Setup all expectations
    EXPECT_CALL(*decoder, Open(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called again when song is resumed
    EXPECT_CALL(*playback, Prepare()).Times(2).WillRepeatedly(Return(error::kSuccess));

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          // Starts playing
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          // Notify other thread to ask for pause and wait for it
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Pause, change device and wait to resume
          position++;
          EXPECT_TRUE(callback(0, 0, 0, 0, position));

          return error::kSuccess;
        }));

    using State = model::Song::MediaState;

    EXPECT_CALL(*playback, Pause());
    EXPECT_CALL(*notifier,
                NotifySongState(Field(&model::Song::CurrentInformation::state, State::Play)))
        .Times(2);
    EXPECT_CALL(*notifier,
                NotifySongState(Field(&model::Song::CurrentInformation::state, State::Pause)))
        .WillOnce(Invoke([&] { syncer.NotifyStep(4); }));

    // New playback stream is created while song is paused (and it stays paused)
    EXPECT_CALL(*playback, CreatePlaybackStream(device)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, ConfigureParameters(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, GetPeriodSize()).WillOnce(Invoke([&] {
      syncer.NotifyStep(5);
      return 0;
    }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(2);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(2);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}));
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(6);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    player_ctl->Play(std::string{"The Weeknd - Blinding Lights"});

    // Wait until Player starts decoding before client asks to pause
    syncer.WaitForStep(2);
    player_ctl->PauseOrResume();
    syncer.NotifyStep(3);

    // Change device after player is paused
    syncer.WaitForStep(4);
    player_ctl->SetAudioDevice(device);

    // Resume after device is changed
    syncer.WaitForStep(5);
    player_ctl->PauseOrResume();

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(6);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingSeekForwardAndBackward) {
  const std::string song{"Mareux - Summertime"};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Setup all expectations
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, song)))
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
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          syncer.NotifyStep(2);
          callback(0, 0, 0, 0, position);
          syncer.WaitForStep(3);

          for (int i = 0; i <= 3; i++) {
            position++;
            callback(0, 0, 0, 0, position);
          }

          // This value is considering the seek backward/forward commands + sum in the for-loop
          EXPECT_EQ(5, position);

          return error::kSuccess;
        }));

    // These methods should be called only one time because of seek backward/forward command
    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(2);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(2);
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(2);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}));
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

TEST_F(PlayerTest, SeekWhilePaused) {
  const std::string song{"Joji - Glimpse of Us"};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Setup all expectations
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, song)))
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
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          for (int i = 0; i <= 3; i++) {
            position++;
            callback(0, 0, 0, 0, position);
          }

          // This value is considering the seek forward commands received while song was paused
          // (which are used by decoder as soon as song is resumed) + sum in the for-loop
          EXPECT_EQ(7, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*playback, Pause());

    // Samples from the position where song was paused are not played after changing position
    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(4);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(4);

    // Using-declaration to improve readability
    using State = model::Song::MediaState;

    // This is called 4 times because of position update notification
    EXPECT_CALL(*notifier,
                NotifySongState(Field(&model::Song::CurrentInformation::state, State::Play)))
        .Times(4);

    // Song is paused with the last position notified, and each position changed while paused is
    // notified right away (so interface does not have to wait until song is resumed to show it)
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{.state = State::Pause,
                                                                           .position = 0}))
        .WillOnce(Invoke([&] { syncer.NotifyStep(4); }));

    for (uint32_t position : {2, 3, 4}) {
      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = State::Pause, .position = position}));
    }

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}));
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
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_filename1)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare is called before start playing
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself. So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 1;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          position++;
          callback(0, 0, 0, 0, position);

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

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}))
        .Times(0);  // This must not be called at all
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      /* ************************************************************************************** */
      // ATTENTION: this is the workaround found to iterate in a new audio loop to play (using the
      // invoke function to call it from the previous loop)

      // Setup expectation for second song
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_filename2)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(_));

      // Prepare is called before start playing
      EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

      // In this case, this won't even play at all, it will wait for the Exit command from client
      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;

            syncer.NotifyStep(4);
            syncer.WaitForStep(5);

            callback(0, 0, 0, 0, position);
            return error::kSuccess;
          }));

      EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
      EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

      expected_position = 0;
      EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::position,
                                                   expected_position)))
          .Times(0);

      // These are called by Player::ResetMediaControl()
      EXPECT_CALL(*decoder, ClearCache());
      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = model::Song::MediaState::Finished}))
          .Times(0);  // This must not be called at all
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
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_filename1)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare() should be called only once, because when we receive a new Play command,
    // it should exit from loop
    EXPECT_CALL(*playback, Prepare()).Times(1).WillRepeatedly(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself. So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 1;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // This next callback call will be blocked until receives some of the expected commands
          // for Paused state
          position++;
          callback(0, 0, 0, 0, position);

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

    // Song is notified as paused, and once again when its position is changed while paused (seek
    // forward is not possible, as duration from this song is not known)
    EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::state,
                                                 model::Song::MediaState::Pause)))
        .Times(2);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}))
        .Times(0);  // Song was interrupted by a new one, otherwise UI would skip to next file
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      /* ************************************************************************************** */
      // ATTENTION: this is the workaround found to iterate in a new audio loop to play (using the
      // invoke function to call it from the previous loop)

      // Setup expectation for second song
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_filename2)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(_));

      // Prepare is called before start playing
      EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

      // In this case, this won't even play at all, it will wait for the Exit command from client
      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;

            syncer.NotifyStep(4);
            syncer.WaitForStep(5);

            callback(0, 0, 0, 0, position);
            return error::kSuccess;
          }));

      EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
      EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

      expected_position = 0;
      EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::position,
                                                   expected_position)))
          .Times(0);

      // These are called by Player::ResetMediaControl()
      EXPECT_CALL(*decoder, ClearCache());
      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = model::Song::MediaState::Finished}))
          .Times(0);  // This must not be called at all
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

    // Send any command: position may be changed while paused, anything else is ignored by audio
    // thread
    player_ctl->SeekForwardPosition(1);
    player_ctl->SeekBackwardPosition(1);
    player_ctl->SetAudioVolume(model::Volume{0.5f});

    // Now send a new song request
    player_ctl->Play(filename2);

    // Wait for Player to finish playing song before client asks to exit
    syncer.WaitForStep(4);
    player_ctl->Exit();
    syncer.NotifyStep(5);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StartPlayingThenPauseAndUpdateAudioFilters) {
  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Received filepaths to play
    const std::string expected_filename{"gelowler - Middle Of The Night"};

    /* ****************************************************************************************** */
    // Setup expectation for first song
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, expected_filename)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));

    // Prepare() should be called only once, because when we receive a new Play command,
    // it should exit from loop
    EXPECT_CALL(*playback, Prepare()).Times(1).WillRepeatedly(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself. So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 1;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // This next callback call will be blocked until receives some of the expected commands
          // for Paused state
          position++;
          callback(0, 0, 0, 0, position);

          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(2);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(2);

    model::EqualizerPreset expected_preset = model::AudioFilter::CreatePresets()["Custom"];
    EXPECT_CALL(*decoder, UpdateFilters(expected_preset));

    // In this case, decoder will tell us that the current timestamp matches some position other
    // than zero (this value is represented in seconds). And for this, we should notify Media
    // Player to update its graphical interface
    EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::position, _)))
        .Times(2);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}));
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      syncer.NotifyStep(4);
    }));

    /* ****************************************************************************************** */
    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    const std::string filename{"gelowler - Middle Of The Night"};

    // Ask Audio Player to play file
    player_ctl->Play(filename);

    // Wait until Player starts decoding to update audio filters
    syncer.WaitForStep(2);

    model::EqualizerPreset preset = model::AudioFilter::CreatePresets()["Custom"];
    player_ctl->ApplyAudioFilters(preset);
    syncer.NotifyStep(3);

    // Wait a bit, just until Player executes the audio filters update
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Wait for Player to finish updating audio filters before client asks to exit
    syncer.WaitForStep(4);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorOpeningSongFromPlaylistPlayNextAndExit) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Chill mix",
      .songs =
          {
              model::Song{.filepath = "chilling 1.mp3"},
              model::Song{.filepath = "chilling 2.mp3"},
              model::Song{.filepath = "chilling 3.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    InSequence seq;

    // Setup all expectations for error on file opening
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[0].filepath)))
        .WillOnce(Return(error::kFileNotSupported));

    // Only these should be called on error
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kFileNotSupported), StrEq("chilling 1.mp3")));
    EXPECT_CALL(*notifier, ClearSongInformation(false)).Times(0);

    // Setup expectations for playing next song
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[1].filepath)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself. So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);
          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _));
    EXPECT_CALL(*playback, AudioCallback(_, _));

    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Play, .position = 0}))
        .WillOnce(Invoke([&] {
          // Notify other thread from here, so we can skip song 3 and force to exit
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);
        }));

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}))
        .Times(0);  // This is not called because of internal media control state which is Exit
    EXPECT_CALL(*notifier, ClearSongInformation(true));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    // First song gets an error (shown as warning to user), so audio player plays next song itself.
    // Wait for Player to start playing second song before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
    syncer.NotifyStep(3);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorDecodingSongFromPlaylistPlayNextAndExit) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Chill mix",
      .songs =
          {
              model::Song{.filepath = "chilling 1.mp3"},
              model::Song{.filepath = "chilling 2.mp3"},
              model::Song{.filepath = "chilling 3.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    InSequence seq;

    // Setup all expectations for error on file decoding
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[0].filepath)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*decoder, Decode(_, _)).WillOnce(Return(error::kDecodeFileFailed));

    // This should not be called in this situation
    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

    // Only these should be called on error
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, ClearSongInformation(true));
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kDecodeFileFailed), StrEq("chilling 1.mp3")));

    // Setup expectations for playing next song
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[1].filepath)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself. So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);
          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _));
    EXPECT_CALL(*playback, AudioCallback(_, _));

    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Play, .position = 0}))
        .WillOnce(Invoke([&] {
          // Notify other thread from here, so we can skip song 3 and force to exit
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);
        }));

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}))
        .Times(0);  // This is not called because of internal media control state which is Exit
    EXPECT_CALL(*notifier, ClearSongInformation(true));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    // First song gets an error (shown as warning to user), so audio player plays next song itself.
    // Wait for Player to start playing second song before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
    syncer.NotifyStep(3);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorUpdatingAudioFiltersKeepsPlayingPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Lofi mix",
      .songs =
          {
              model::Song{.filepath = "lofi 1.mp3"},
              model::Song{.filepath = "lofi 2.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[0].filepath)))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[1].filepath)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_)).Times(2);
    EXPECT_CALL(*playback, Prepare()).Times(2).WillRepeatedly(Return(error::kSuccess));

    // Decoding first song: audio filters are updated (and fail) in the middle of it
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 1;
          callback(0, 0, 0, 0, position);

          // Wait for client to ask for audio filters update
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Command to update audio filters is handled here (and it fails)
          position++;
          callback(0, 0, 0, 0, position);

          // Wait for client to close error dialog (which asks to dequeue next song)
          syncer.WaitForStep(5);

          // Current song must keep playing
          position++;
          EXPECT_TRUE(callback(0, 0, 0, 0, position));

          return error::kSuccess;
        }))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(3);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(3);

    EXPECT_CALL(*decoder, UpdateFilters(_)).WillOnce(Return(error::kEqualizerFailed));
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kEqualizerFailed), StrEq("")))
        .WillOnce(Invoke([&] { syncer.NotifyStep(4); }));

    EXPECT_CALL(*notifier, NotifySongState(Field(&model::Song::CurrentInformation::state,
                                                 model::Song::MediaState::Play)))
        .Times(3);

    // Both songs finish normally
    EXPECT_CALL(*decoder, ClearCache()).Times(2);
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}))
        .Times(2);
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Return()).WillOnce(Invoke([&] {
      syncer.NotifyStep(6);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    player_ctl->Play(playlist);

    // Update audio filters while first song is playing
    syncer.WaitForStep(2);
    player_ctl->ApplyAudioFilters(model::AudioFilter::CreatePresets()["Custom"]);
    syncer.NotifyStep(3);

    // Error dialog is closed by user, but song did not stop, so next one must not be dequeued
    syncer.WaitForStep(4);
    player_ctl->DequeueNextSong();
    syncer.NotifyStep(5);

    // Wait for both songs to finish before client asks to exit
    syncer.WaitForStep(6);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, PlaySongFilesFromPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Breakcore mix",
      .songs =
          {
              model::Song{.filepath = "breakcore 1.mp3"},
              model::Song{.filepath = "breakcore 2.mp3"},
              model::Song{.filepath = "breakcore 3.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    InSequence seq;

    for (int i = 0; i < 3; ++i) {
      // Setup all expectations for playing all songs in sequence
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[i].filepath)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(_));
      EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

      // Only interested in second argument, which is a lambda created internally by audio_player
      // itself. So it is necessary to manually call it, to keep the behaviour similar to a
      // real-life situation
      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;
            callback(0, 0, 0, 0, position);
            return error::kSuccess;
          }));

      EXPECT_CALL(*notifier, SendAudioRaw(_, _));
      EXPECT_CALL(*playback, AudioCallback(_, _));

      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = model::Song::MediaState::Play, .position = 0}));

      // These are called by Player::ResetMediaControl()
      EXPECT_CALL(*decoder, ClearCache());
      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = model::Song::MediaState::Finished}));

      // Exit only after the last song has finished (otherwise, exit could arrive before song
      // finishes, and then it would not be notified as finished)
      EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&, i] {
        if (i == 2) syncer.NotifyStep(2);
      }));
    }

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    // Wait for Player to play all songs before client asking to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, PlaySongsWithDifferentSampleRates) {
  using model::SampleFormat;

  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Hi-res mix",
      .songs =
          {
              model::Song{.filepath = "hi-res 1.flac"},
              model::Song{.filepath = "hi-res 2.flac"},
              model::Song{.filepath = "cd quality.flac"},
              model::Song{.filepath = "unknown.flac"},
          },
  };

  // Sample rate filled by decoder for each song (the last one is unknown)
  const std::vector<uint32_t> sample_rates{96000, 96000, 44100, 0};

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    // Format expected by playback is always the last one that it was asked for (as if output
    // device supports all of them)
    model::AudioFormat format;
    EXPECT_CALL(*playback, GetFormat()).WillRepeatedly(Invoke([&] { return format; }));

    // Playback is configured again only when sample rate is not the same one from previous song
    // (always asking for the widest sample format)
    for (uint32_t sample_rate : {96000U, 44100U}) {
      const model::AudioFormat desired{.sample_rate = sample_rate,
                                       .sample_format = SampleFormat::S32};

      EXPECT_CALL(*playback, ConfigureParameters(Eq(desired)))
          .WillOnce(Invoke([&](const model::AudioFormat& value) {
            format = value;
            return error::kSuccess;
          }));
    }

    EXPECT_CALL(*playback, GetPeriodSize()).Times(2);

    InSequence seq;

    for (size_t i = 0; i < playlist.songs.size(); ++i) {
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[i].filepath)))
          .WillOnce(Invoke([&, i](model::Song& song) {
            song.sample_rate = sample_rates[i];
            return error::kSuccess;
          }));

      // And decoder is asked for samples in the format expected by playback (for a song with
      // unknown sample rate, it is the same one from previous song)
      const model::AudioFormat expected{
          .sample_rate = sample_rates[i] > 0 ? sample_rates[i] : sample_rates[i - 1],
          .sample_format = SampleFormat::S32};

      EXPECT_CALL(*decoder, SetOutputFormat(Eq(expected))).WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(_));
      EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;
            callback(0, 0, 0, 0, position);
            return error::kSuccess;
          }));

      EXPECT_CALL(*playback, AudioCallback(_, _));
      EXPECT_CALL(*notifier, NotifySongState(_));

      // These are called by Player::ResetMediaControl()
      EXPECT_CALL(*decoder, ClearCache());
      EXPECT_CALL(*notifier, NotifySongState(_));
      EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&, i] {
        if (i + 1 == playlist.songs.size()) syncer.NotifyStep(2);
      }));
    }

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    player_ctl->Play(playlist);

    // Wait for Player to play all songs before client asking to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorConfiguringPlaybackForSong) {
  using model::SampleFormat;

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    EXPECT_CALL(*decoder, Open(_)).WillOnce(Invoke([](model::Song& song) {
      song.sample_rate = 192000;
      return error::kSuccess;
    }));

    // Playback cannot be configured with the format from song, so its stream is created again
    // with the previous one (otherwise, no other song could be played)
    InSequence seq;

    EXPECT_CALL(*playback, ConfigureParameters(Eq(model::AudioFormat{
                               .sample_rate = 192000, .sample_format = SampleFormat::S32})))
        .WillOnce(Return(error::kSetupAudioParamsFailed));

    EXPECT_CALL(*playback, CreatePlaybackStream("")).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback,
                ConfigureParameters(Eq(model::AudioFormat{.sample_format = SampleFormat::S32})))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*playback, GetPeriodSize());

    // And song is not played
    EXPECT_CALL(*decoder, SetOutputFormat(_)).Times(0);
    EXPECT_CALL(*decoder, Decode(_, _)).Times(0);
    EXPECT_CALL(*playback, Prepare()).Times(0);
    EXPECT_CALL(*notifier, NotifySongInformation(_)).Times(0);

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kSetupAudioParamsFailed), _)).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    player_ctl->Play(std::string{"The Weeknd - Blinding Lights"});

    // Wait for Player to notify error before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, PlayStreamingSongsFromPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Drum and bass mix",
      .songs = {model::Song{.stream_info =
                                model::StreamInfo{.base_url = "https://somecrazysite.com/song"}},
                model::Song{.stream_info =
                                model::StreamInfo{.base_url = "https://yotubio.com/drum"}},
                model::Song{.stream_info =
                                model::StreamInfo{.base_url = "https://guugleo.com/drop"}}},
  };

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();
    auto fetcher = GetStreamFetcher();

    InSequence seq;

    for (int i = 0; i < 3; ++i) {
      // Setup all expectations for playing all 3 songs from streaming in sequence
      EXPECT_CALL(*fetcher, ExtractInfo(_)).WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*decoder, Open(Field(&model::Song::stream_info, playlist.songs[i].stream_info)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(_));
      EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

      // Only interested in second argument, which is a lambda created internally by audio_player
      // itself. So it is necessary to manually call it, to keep the behaviour similar to a
      // real-life situation
      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;
            callback(0, 0, 0, 0, position);
            return error::kSuccess;
          }));

      EXPECT_CALL(*notifier, SendAudioRaw(_, _));
      EXPECT_CALL(*playback, AudioCallback(_, _));

      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = model::Song::MediaState::Play, .position = 0}));

      // These are called by Player::ResetMediaControl()
      EXPECT_CALL(*decoder, ClearCache());
      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = model::Song::MediaState::Finished}));

      // Exit only after the last song has finished (otherwise, exit could arrive before song
      // finishes, and then it would not be notified as finished)
      EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&, i] {
        if (i == 2) syncer.NotifyStep(2);
      }));
    }

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    // Wait for Player to play all songs before client asking to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ErrorFetchingSongFromPlaylistPlayNextAndExit) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Metalcore",
      .songs = {model::Song{.stream_info =
                                model::StreamInfo{.base_url = "https://killer.song/music"}},
                model::Song{.stream_info =
                                model::StreamInfo{.base_url = "https://streaming.site/yeah"}},
                model::Song{.stream_info = model::StreamInfo{.base_url = "ftp://what.is/this"}}},
  };

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();
    auto fetcher = GetStreamFetcher();

    InSequence seq;

    // Setup all expectations for error on fetching song info
    EXPECT_CALL(*fetcher, ExtractInfo(_)).WillOnce(Return(error::kStreamFetchFailed));

    // This should not be called in this situation
    EXPECT_CALL(*decoder, Open(Field(&model::Song::stream_info, playlist.songs[0].stream_info)))
        .Times(0);
    EXPECT_CALL(*notifier, NotifySongInformation(_)).Times(0);
    EXPECT_CALL(*playback, Prepare()).Times(0);
    EXPECT_CALL(*decoder, Decode(_, _)).Times(0);
    EXPECT_CALL(*notifier, SendAudioRaw(_, _)).Times(0);
    EXPECT_CALL(*playback, AudioCallback(_, _)).Times(0);

    // Only these should be called on error
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kStreamFetchFailed), _));
    EXPECT_CALL(*notifier, ClearSongInformation(true)).Times(0);

    // Setup expectations for playing next song
    EXPECT_CALL(*fetcher, ExtractInfo(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, Open(Field(&model::Song::stream_info, playlist.songs[1].stream_info)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*notifier, NotifySongInformation(_));
    EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

    // Only interested in second argument, which is a lambda created internally by audio_player
    // itself. So it is necessary to manually call it, to keep the behaviour similar to a
    // real-life situation
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);
          return error::kSuccess;
        }));

    EXPECT_CALL(*notifier, SendAudioRaw(_, _));
    EXPECT_CALL(*playback, AudioCallback(_, _));

    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Play, .position = 0}))
        .WillOnce(Invoke([&] {
          // Notify other thread from here, so we can skip song 3 and force to exit
          syncer.NotifyStep(2);
          syncer.WaitForStep(3);
        }));

    // These are called by Player::ResetMediaControl()
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}))
        .Times(0);  // This is not called because of internal media control state which is Exit
    EXPECT_CALL(*notifier, ClearSongInformation(true));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    // First song gets an error (shown as warning to user), so audio player plays next song itself.
    // Wait for Player to start playing second song before client asks to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
    syncer.NotifyStep(3);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StopPlaylistAfterSeveralFailedSongs) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Broken mix",
      .songs =
          {
              model::Song{.filepath = "broken 1.mp3"},
              model::Song{.filepath = "broken 2.mp3"},
              model::Song{.filepath = "broken 3.mp3"},
              model::Song{.filepath = "fine 4.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();

    InSequence seq;

    // Each song fails with a warning, so player tries the next one by itself
    const error::Code errors[] = {error::kFileNotSupported, error::kInvalidFile,
                                  error::kCorruptedData};

    for (int i = 0; i < 3; ++i) {
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[i].filepath)))
          .WillOnce(Return(errors[i]));
      EXPECT_CALL(*decoder, ClearCache());
      EXPECT_CALL(*notifier, NotifyError(Eq(errors[i]), StrEq(playlist.songs[i].filepath)));
    }

    // After the third song failing in a row, playlist is stopped and user is notified about it
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kTooManyFailedSongs), StrEq("")))
        .WillOnce(Invoke([&] { syncer.NotifyStep(2); }));

    // Last song is never played
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[3].filepath))).Times(0);

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    // Closing error dialog must not play anything else, as playlist was stopped
    syncer.WaitForStep(2);
    player_ctl->DequeueNextSong();
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, FetchSongFromUrlAgainWhenInformationKeptIsNotAccepted) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Streaming mix",
      .songs = {model::Song{.stream_info = model::StreamInfo{.base_url = "https://site/first"}}},
  };

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();
    auto fetcher = GetStreamFetcher();

    InSequence seq;

    // Fetcher gives the information kept from the last time, but its URL is not accepted anymore
    EXPECT_CALL(*fetcher, ExtractInfo(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, Open(_)).WillOnce(Return(error::kFileNotSupported));
    EXPECT_CALL(*fetcher, Forget(_)).WillOnce(Return(true));

    // So it is fetched again, but only once (even if it fails again)
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*fetcher, ExtractInfo(_)).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, Open(_)).WillOnce(Return(error::kFileNotSupported));

    EXPECT_CALL(*fetcher, Forget(_)).Times(0);
    EXPECT_CALL(*fetcher, ExtractInfo(_)).Times(0);

    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kFileNotSupported), _)).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StopPlaylistWhenSongsFromUrlAreRefused) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Streaming mix",
      .songs =
          {
              model::Song{.stream_info = model::StreamInfo{.base_url = "https://site/first"}},
              model::Song{.stream_info = model::StreamInfo{.base_url = "https://site/second"}},
              model::Song{.stream_info = model::StreamInfo{.base_url = "https://site/third"}},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();
    auto fetcher = GetStreamFetcher();

    InSequence seq;

    // Site refuses the very first request, so there is no reason to ask for the other songs
    EXPECT_CALL(*fetcher, ExtractInfo(_)).WillOnce(Return(error::kStreamBlocked));
    EXPECT_CALL(*decoder, ClearCache());
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kStreamBlocked), _)).WillOnce(Invoke([&] {
      syncer.NotifyStep(2);
    }));

    EXPECT_CALL(*fetcher, ExtractInfo(_)).Times(0);
    EXPECT_CALL(*decoder, Open(_)).Times(0);
    EXPECT_CALL(*notifier, NotifyError(Eq(error::kTooManyFailedSongs), _)).Times(0);

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    // Closing error dialog must not play anything else, as playlist was stopped
    syncer.WaitForStep(2);
    player_ctl->DequeueNextSong();
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, KeepPlayingPlaylistWhenFailedSongsAreNotInRow) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Mixed bag",
      .songs =
          {
              model::Song{.filepath = "broken 1.mp3"},
              model::Song{.filepath = "broken 2.mp3"},
              model::Song{.filepath = "fine 3.mp3"},
              model::Song{.filepath = "broken 4.mp3"},
              model::Song{.filepath = "fine 5.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto playback = GetPlayback();
    auto decoder = GetDecoder();

    InSequence seq;

    // Song fails with a warning, so player tries the next one by itself
    auto expect_failure = [&](int i) {
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[i].filepath)))
          .WillOnce(Return(error::kFileNotSupported));
      EXPECT_CALL(*decoder, ClearCache());
      EXPECT_CALL(*notifier,
                  NotifyError(Eq(error::kFileNotSupported), StrEq(playlist.songs[i].filepath)));
    };

    // Song is played until its end
    auto expect_success = [&](int i) {
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[i].filepath)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(_));
      EXPECT_CALL(*playback, Prepare()).WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;
            callback(0, 0, 0, 0, position);
            return error::kSuccess;
          }));

      EXPECT_CALL(*notifier, SendAudioRaw(_, _));
      EXPECT_CALL(*playback, AudioCallback(_, _));
      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = model::Song::MediaState::Play, .position = 0}));

      // These are called by Player::ResetMediaControl()
      EXPECT_CALL(*decoder, ClearCache());
      EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                                 .state = model::Song::MediaState::Finished}));
      EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&, i] {
        if (i == 4) syncer.NotifyStep(2);
      }));
    };

    // Song played successfully resets count of failed songs, so playlist is never stopped
    expect_failure(0);
    expect_failure(1);
    expect_success(2);
    expect_failure(3);
    expect_success(4);

    EXPECT_CALL(*notifier, NotifyError(Eq(error::kTooManyFailedSongs), _)).Times(0);

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);

    // Ask Audio Player to play
    player_ctl->Play(playlist);

    // Wait for Player to play all songs before client asking to exit
    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, SkipToNextAndPreviousInPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Skippy",
      .songs =
          {
              model::Song{.filepath = "skippy 1.mp3"},
              model::Song{.filepath = "skippy 2.mp3"},
              model::Song{.filepath = "skippy 3.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();

    EXPECT_CALL(*GetPlayback(), Prepare()).WillRepeatedly(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(AnyNumber());

    // Songs are skipped (not finished), so player never plays next song by itself
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}))
        .Times(0);

    InSequence seq;

    // Play song from playlist, notify client and wait for its command (to skip song or exit)
    auto expect_song = [&](int index, int step) {
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[index].filepath)))
          .WillOnce(Return(error::kSuccess));

      EXPECT_CALL(*notifier, NotifySongInformation(AllOf(
                                 Field(&model::Song::filepath, playlist.songs[index].filepath),
                                 Field(&model::Song::playlist, Eq(playlist.name)))));

      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([&, step](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;
            callback(0, 0, 0, 0, position);

            syncer.NotifyStep(step);
            syncer.WaitForStep(step + 1);

            // Command from client stops this song
            EXPECT_FALSE(callback(0, 0, 0, 0, position));
            return error::kSuccess;
          }));
    };

    expect_song(0, 2);  // Start playing first song
    expect_song(1, 4);  // Skip to next
    expect_song(0, 6);  // Skip to previous
    expect_song(0, 8);  // Skip to previous on first song plays it again

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    player_ctl->Play(playlist);

    syncer.WaitForStep(2);
    player_ctl->SkipToNext();
    syncer.NotifyStep(3);

    syncer.WaitForStep(4);
    player_ctl->SkipToPrevious();
    syncer.NotifyStep(5);

    syncer.WaitForStep(6);
    player_ctl->SkipToPrevious();
    syncer.NotifyStep(7);

    syncer.WaitForStep(8);
    player_ctl->Exit();
    syncer.NotifyStep(9);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, SkipToNextWhilePausedInPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Paused",
      .songs =
          {
              model::Song{.filepath = "paused 1.mp3"},
              model::Song{.filepath = "paused 2.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();

    EXPECT_CALL(*GetPlayback(), Prepare()).WillRepeatedly(Return(error::kSuccess));
    EXPECT_CALL(*GetPlayback(), Pause());
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(AnyNumber());
    EXPECT_CALL(*notifier, NotifySongState(model::Song::CurrentInformation{
                               .state = model::Song::MediaState::Finished}))
        .Times(0);

    InSequence seq;

    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[0].filepath)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Song is paused, and then skipped while paused
          EXPECT_FALSE(callback(0, 0, 0, 0, position));
          return error::kSuccess;
        }));

    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[1].filepath)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(4);
          syncer.WaitForStep(5);

          // Exit
          EXPECT_FALSE(callback(0, 0, 0, 0, position));
          return error::kSuccess;
        }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    player_ctl->Play(playlist);

    syncer.WaitForStep(2);
    player_ctl->PauseOrResume();
    player_ctl->SkipToNext();
    syncer.NotifyStep(3);

    syncer.WaitForStep(4);
    player_ctl->Exit();
    syncer.NotifyStep(5);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, SkipToNextIsIgnoredForSingleFile) {
  CheckSkipIsIgnored([](audio::AudioControl& player) { player.Play("single.mp3"); }, "single.mp3",
                     {/*skip_to_next=*/true});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, SkipToNextIsIgnoredOnLastSongFromPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Lonely",
      .songs = {model::Song{.filepath = "lonely.mp3"}},
  };

  CheckSkipIsIgnored([&](audio::AudioControl& player) { player.Play(playlist); }, "lonely.mp3",
                     {/*skip_to_next=*/true});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, StopClearsPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Stopped",
      .songs =
          {
              model::Song{.filepath = "stopped 1.mp3"},
              model::Song{.filepath = "stopped 2.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();

    EXPECT_CALL(*GetPlayback(), Prepare()).WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(AnyNumber());

    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[0].filepath)))
        .WillOnce(Return(error::kSuccess));

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Stop
          EXPECT_FALSE(callback(0, 0, 0, 0, position));
          return error::kSuccess;
        }));

    // Next song must not be played after user stops playing
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[1].filepath))).Times(0);

    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillOnce(Invoke([&] {
      EXPECT_FALSE(HasPlaylist());
      syncer.NotifyStep(4);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    player_ctl->Play(playlist);

    syncer.WaitForStep(2);
    player_ctl->Stop();
    syncer.NotifyStep(3);

    syncer.WaitForStep(4);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, RepeatOneSongFromPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Repeated",
      .songs =
          {
              model::Song{.filepath = "repeat 1.mp3"},
              model::Song{.filepath = "repeat 2.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();

    EXPECT_CALL(*GetPlayback(), Prepare()).WillRepeatedly(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(AnyNumber());

    InSequence seq;

    // First song finishes, so it is played again
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[0].filepath)))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);
          return error::kSuccess;
        }));

    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[0].filepath)))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Skip to next song (user can still skip songs while repeating one)
          EXPECT_FALSE(callback(0, 0, 0, 0, position));
          return error::kSuccess;
        }));

    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[1].filepath)))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(4);
          syncer.WaitForStep(5);

          // Exit
          EXPECT_FALSE(callback(0, 0, 0, 0, position));
          return error::kSuccess;
        }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    player_ctl->SetRepeatMode(model::RepeatMode::One);
    player_ctl->Play(playlist);

    syncer.WaitForStep(2);
    player_ctl->SkipToNext();
    syncer.NotifyStep(3);

    syncer.WaitForStep(4);
    player_ctl->Exit();
    syncer.NotifyStep(5);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, RepeatAllSongsFromPlaylist) {
  model::Playlist playlist = model::Playlist{
      .index = 0,
      .name = "Looping",
      .songs =
          {
              model::Song{.filepath = "loop 1.mp3"},
              model::Song{.filepath = "loop 2.mp3"},
          },
  };

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();

    EXPECT_CALL(*GetPlayback(), Prepare()).WillRepeatedly(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(AnyNumber());

    InSequence seq;

    // Both songs finish
    for (int i = 0; i < 2; ++i) {
      EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[i].filepath)))
          .WillOnce(Return(error::kSuccess));
      EXPECT_CALL(*decoder, Decode(_, _))
          .WillOnce(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
            int64_t position = 0;
            callback(0, 0, 0, 0, position);
            return error::kSuccess;
          }));
    }

    // After last song, first one is played again
    EXPECT_CALL(*decoder, Open(Field(&model::Song::filepath, playlist.songs[0].filepath)))
        .WillOnce(Return(error::kSuccess));
    EXPECT_CALL(*decoder, Decode(_, _))
        .WillOnce(Invoke([&](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);

          syncer.NotifyStep(2);
          syncer.WaitForStep(3);

          // Exit
          EXPECT_FALSE(callback(0, 0, 0, 0, position));
          return error::kSuccess;
        }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    player_ctl->SetRepeatMode(model::RepeatMode::All);
    player_ctl->Play(playlist);

    syncer.WaitForStep(2);
    player_ctl->Exit();
    syncer.NotifyStep(3);
  };

  testing::RunAsyncTest({player, client});
}

/* ********************************************************************************************** */

TEST_F(PlayerTest, ShuffleSongsFromPlaylist) {
  model::Playlist playlist = model::Playlist{.index = 0, .name = "Shuffled"};
  for (int i = 0; i < 8; ++i) {
    playlist.songs.push_back(model::Song{.filepath = "song " + std::to_string(i) + ".mp3"});
  }

  std::vector<std::filesystem::path> played;

  auto player = [&](TestSyncer& syncer) {
    auto decoder = GetDecoder();

    EXPECT_CALL(*GetPlayback(), Prepare()).WillRepeatedly(Return(error::kSuccess));
    EXPECT_CALL(*notifier, NotifySongState(_)).Times(AnyNumber());

    // Every song is played until its end
    EXPECT_CALL(*decoder, Open(_))
        .Times(static_cast<int>(playlist.songs.size()))
        .WillRepeatedly(Invoke([&](model::Song& song) {
          played.push_back(song.filepath);
          return error::kSuccess;
        }));

    EXPECT_CALL(*decoder, Decode(_, _))
        .WillRepeatedly(Invoke([](int dummy, audio::Decoder::AudioCallback callback) {
          int64_t position = 0;
          callback(0, 0, 0, 0, position);
          return error::kSuccess;
        }));

    // Exit only after the last song has finished
    EXPECT_CALL(*notifier, ClearSongInformation(true)).WillRepeatedly(Invoke([&] {
      if (played.size() == playlist.songs.size()) syncer.NotifyStep(2);
    }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAudioLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto player_ctl = GetAudioControl();
    syncer.WaitForStep(1);
    player_ctl->SetShuffle(true);
    player_ctl->Play(playlist);

    syncer.WaitForStep(2);
    player_ctl->Exit();
  };

  testing::RunAsyncTest({player, client});

  // Selected song is played first, then every other song is played once (in any order)
  ASSERT_EQ(played.size(), playlist.songs.size());
  EXPECT_EQ(played.front(), playlist.songs.front().filepath);

  std::vector<std::filesystem::path> expected;
  for (const auto& song : playlist.songs) expected.push_back(song.filepath);

  std::sort(played.begin(), played.end());
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(played, expected);
}

/* ********************************************************************************************** */

//! Utility to get the pretty print from any value
template <typename T>
std::string Print(const T& value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

/* ********************************************************************************************** */

TEST(CommandTest, PrintIdentifier) {
  using audio::Command;
  const std::vector<std::pair<Command::Identifier, std::string>> expected{
      {Command::Identifier::None, "None"},
      {Command::Identifier::Play, "Play"},
      {Command::Identifier::PauseOrResume, "PauseOrResume"},
      {Command::Identifier::Stop, "Stop"},
      {Command::Identifier::SeekForward, "SeekForward"},
      {Command::Identifier::SeekBackward, "SeekBackward"},
      {Command::Identifier::SetVolume, "SetVolume"},
      {Command::Identifier::UpdateAudioFilters, "UpdateAudioFilter"},
      {Command::Identifier::Exit, "Exit"},
      {Command::Identifier::SkipToNext, "SkipToNext"},
      {Command::Identifier::SkipToPrevious, "SkipToPrevious"},
      {Command::Identifier::PlayNext, "PlayNext"},
  };

  for (const auto& [id, name] : expected) {
    EXPECT_THAT(Print(id), ::testing::StrEq(name));
  }
}

/* ********************************************************************************************** */

TEST(CommandTest, PrintListOfIdentifiers) {
  using audio::Command;
  using Identifiers = std::vector<Command::Identifier>;

  EXPECT_THAT(Print(Identifiers{}), ::testing::StrEq("[]"));
  EXPECT_THAT(Print(Identifiers{Command::Identifier::Play}), ::testing::StrEq(R"(["Play"])"));
  EXPECT_THAT(Print(Identifiers{Command::Identifier::Play, Command::Identifier::Stop,
                                Command::Identifier::Exit}),
              ::testing::StrEq(R"(["Play","Stop","Exit"])"));
}

/* ********************************************************************************************** */

TEST(CommandTest, PrintCommands) {
  using audio::Command;
  using Commands = std::vector<Command>;

  EXPECT_THAT(Print(Command::PauseOrResume()), ::testing::StrEq("PauseOrResume"));

  EXPECT_THAT(Print(Commands{}), ::testing::StrEq("[]"));
  EXPECT_THAT(Print(Commands{Command::Stop()}), ::testing::StrEq(R"(["Stop"])"));
  EXPECT_THAT(Print(Commands{Command::SkipToNext(), Command::SkipToPrevious(), Command::None()}),
              ::testing::StrEq(R"(["SkipToNext","SkipToPrevious","None"])"));
}

/* ********************************************************************************************** */

TEST(CommandTest, CreateCommandsWithoutContent) {
  using audio::Command;
  const std::vector<std::pair<Command, Command::Identifier>> commands{
      {Command::None(), Command::Identifier::None},
      {Command::PauseOrResume(), Command::Identifier::PauseOrResume},
      {Command::Stop(), Command::Identifier::Stop},
      {Command::Exit(), Command::Identifier::Exit},
      {Command::SkipToNext(), Command::Identifier::SkipToNext},
      {Command::SkipToPrevious(), Command::Identifier::SkipToPrevious},
      {Command::PlayNext(), Command::Identifier::PlayNext},
  };

  for (const auto& [command, id] : commands) {
    EXPECT_EQ(command.GetId(), id);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(command.content));
  }
}

/* ********************************************************************************************** */

TEST(CommandTest, CreateCommandsWithContent) {
  using audio::Command;
  const model::Song song{.filepath = "/some/path/to/song.mp3"};
  auto play_song = Command::Play(song);
  EXPECT_EQ(play_song.GetId(), Command::Identifier::Play);
  EXPECT_EQ(play_song.GetContent<model::Song>().filepath, song.filepath);

  const model::Playlist playlist{.index = 3, .name = "coding", .songs = {song}};
  auto play_playlist = Command::Play(playlist);
  EXPECT_EQ(play_playlist.GetId(), Command::Identifier::Play);
  EXPECT_EQ(play_playlist.GetContent<model::Playlist>(), playlist);

  auto forward = Command::SeekForward(5);
  EXPECT_EQ(forward.GetId(), Command::Identifier::SeekForward);
  EXPECT_EQ(forward.GetContent<int>(), 5);

  auto backward = Command::SeekBackward(3);
  EXPECT_EQ(backward.GetId(), Command::Identifier::SeekBackward);
  EXPECT_EQ(backward.GetContent<int>(), 3);

  const model::Volume volume{0.4F};
  auto set_volume = Command::SetVolume(volume);
  EXPECT_EQ(set_volume.GetId(), Command::Identifier::SetVolume);
  EXPECT_EQ(set_volume.GetContent<model::Volume>(), volume);

  model::EqualizerPreset preset{};
  preset.front().gain = 6;

  auto filters = Command::UpdateAudioFilters(preset);
  EXPECT_EQ(filters.GetId(), Command::Identifier::UpdateAudioFilters);
  EXPECT_EQ(filters.GetContent<model::EqualizerPreset>().front().gain, 6);
}

/* ********************************************************************************************** */

TEST(CommandTest, GetContentWithWrongType) {
  using audio::Command;
  auto command = Command::SeekForward(5);

  // When content does not hold the given type, a default value is returned
  EXPECT_TRUE(command.GetContent<model::Song>().filepath.empty());
  EXPECT_TRUE(command.GetContent<model::Playlist>().songs.empty());
}

/* ********************************************************************************************** */

TEST(CommandTest, CompareCommands) {
  using audio::Command;
  // Content is not considered on comparison, only its identifier
  EXPECT_TRUE(Command::SeekForward(1) == Command::SeekForward(2));
  EXPECT_TRUE(Command::SeekForward(1) != Command::SeekBackward(1));

  EXPECT_TRUE(Command::Stop() == Command::Identifier::Stop);
  EXPECT_TRUE(Command::Stop() != Command::Identifier::Play);
}

}  // namespace
