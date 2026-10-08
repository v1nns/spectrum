#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "audio/base/notifier.h"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include "general/sync_testing.h"
#include "general/utils.h"
#include "middleware/media_controller.h"
#include "middleware/remote_playlist.h"
#include "mock/analyzer_mock.h"
#include "mock/audio_control_mock.h"
#include "mock/event_dispatcher_mock.h"
#include "mock/file_handler_mock.h"
#include "model/application_error.h"
#include "model/bar_animation.h"
#include "model/block_identifier.h"
#include "model/playlist.h"
#include "model/playlist_operation.h"
#include "model/question_data.h"
#include "model/settings.h"
#include "model/song.h"
#include "util/logger.h"
#include "view/base/custom_event.h"
#include "view/base/keybinding.h"
#include "view/base/notifier.h"
#include "view/base/terminal.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoubleEq;
using ::testing::Each;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Lt;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::VariantWith;

using testing::TestSyncer;

TEST(RemotePlaylistTest, CreateFromTarget) {
  using ::testing::DoAll;
  using ::testing::ElementsAre;
  using ::testing::NiceMock;
  using ::testing::SetArgReferee;

  // Files are listed from a real directory, while saved playlists come from mock
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "spectrum_remote_playlist_test";
  std::filesystem::remove_all(directory);
  std::filesystem::create_directories(directory / "album" / "inner.mp3");
  std::filesystem::create_directories(directory / "empty");

  for (const char* name : {"b.mp3", "a.flac", "c.MP3", "cover.jpg", "notes"}) {
    std::ofstream{directory / "album" / name};
  }

  const model::Playlist saved{
      .index = 3,
      .name = "Summer Eletrohits",
      .songs = {model::Song{.filepath = "/music/kasino.mp3"},
                model::Song{.stream_info = model::StreamInfo{.base_url = "https://youtu.be/abc"}}},
  };

  NiceMock<FileHandlerMock> file_handler;
  ON_CALL(file_handler, ParsePlaylists(_))
      .WillByDefault(DoAll(
          SetArgReferee<0>(model::Playlists{saved, model::Playlist{.index = 4, .name = "Nothing"}}),
          Return(true)));

  //! Get only the name of each file to play
  const auto filenames = [](const model::Playlist& playlist) {
    std::vector<std::string> names;
    for (const auto& song : playlist.songs) names.push_back(song.filepath.filename().string());

    return names;
  };

  // URL is a single song to stream
  EXPECT_TRUE(middleware::IsRemoteUrl("https://www.youtube.com/watch?v=abc"));
  EXPECT_TRUE(middleware::IsRemoteUrl("http://example.com/song.mp3"));
  EXPECT_FALSE(middleware::IsRemoteUrl("/music/https://song.mp3"));
  EXPECT_FALSE(middleware::IsRemoteUrl("Summer Eletrohits"));

  auto playlist = middleware::CreateRemotePlaylist("https://youtu.be/xyz", file_handler);
  ASSERT_TRUE(playlist.has_value());
  ASSERT_EQ(playlist->songs.size(), 1);
  ASSERT_TRUE(playlist->songs.front().stream_info.has_value());
  EXPECT_THAT(playlist->songs.front().stream_info->base_url, StrEq("https://youtu.be/xyz"));

  // Directory has all its media files, in the same order shown by UI
  playlist = middleware::CreateRemotePlaylist((directory / "album").string(), file_handler);
  ASSERT_TRUE(playlist.has_value());
  EXPECT_THAT(filenames(*playlist), ElementsAre("a.flac", "b.mp3", "c.MP3"));

  // File is followed by the other media files from its directory
  playlist =
      middleware::CreateRemotePlaylist((directory / "album" / "b.mp3").string(), file_handler);
  ASSERT_TRUE(playlist.has_value());
  EXPECT_THAT(filenames(*playlist), ElementsAre("b.mp3", "c.MP3", "a.flac"));

  // Even when it does not look like a media file (player is the one to decide about it)
  playlist =
      middleware::CreateRemotePlaylist((directory / "album" / "notes").string(), file_handler);
  ASSERT_TRUE(playlist.has_value());
  EXPECT_THAT(filenames(*playlist), ElementsAre("notes", "a.flac", "b.mp3", "c.MP3"));

  // Name of a saved playlist
  playlist = middleware::CreateRemotePlaylist("Summer Eletrohits", file_handler);
  ASSERT_TRUE(playlist.has_value());
  EXPECT_EQ(*playlist, saved);

  // Nothing to play
  EXPECT_FALSE(middleware::CreateRemotePlaylist((directory / "empty").string(), file_handler));
  EXPECT_FALSE(
      middleware::CreateRemotePlaylist((directory / "missing.mp3").string(), file_handler));
  EXPECT_FALSE(middleware::CreateRemotePlaylist("album/b.mp3", file_handler));
  EXPECT_FALSE(middleware::CreateRemotePlaylist("Nothing", file_handler));
  EXPECT_FALSE(middleware::CreateRemotePlaylist("Unknown", file_handler));

  ON_CALL(file_handler, ParsePlaylists(_)).WillByDefault(Return(false));
  EXPECT_FALSE(middleware::CreateRemotePlaylist("Summer Eletrohits", file_handler));

  std::filesystem::remove_all(directory);
}

/* ********************************************************************************************** */

/**
 * @brief Tests with MediaController class
 */
class MediaControllerTest : public ::testing::Test {
  // using-declarations
  using MediaController = std::shared_ptr<middleware::MediaController>;
  using EventDispatcher = std::shared_ptr<EventDispatcherMock>;
  using AudioControl = std::shared_ptr<AudioControlMock>;
  using Analyzer = std::unique_ptr<AnalyzerMock>;

 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override { Init(); }

  void TearDown() override { controller.reset(); }

  void Init(bool asynchronous = false) {
    // Create mocks
    dispatcher = std::make_shared<EventDispatcherMock>();
    audio_ctl = std::make_shared<AudioControlMock>();
    AnalyzerMock* an_mock = new AnalyzerMock();

    // Setup init expectations
    InSequence seq;

    EXPECT_CALL(*an_mock, Init(Eq(kNumberBars)));

    EXPECT_CALL(*dispatcher,
                ProcessEvent(Field(&interface::CustomEvent::id,
                                   interface::CustomEvent::Identifier::DrawAudioSpectrum)));

    // Create Controller
    controller = middleware::MediaController::Create(dispatcher, audio_ctl, kNumberBars, an_mock,
                                                     asynchronous);
  }

  //! Getter for Player Notifier
  // P.S.: As controller derives from both Notifiers, must use static_cast for upcasting
  auto GetPlayerNotifier() -> audio::Notifier* {
    return static_cast<audio::Notifier*>(controller.get());
  }

  //! Getter for Interface Notifier
  auto GetInterfaceNotifier() -> interface::Notifier* {
    return static_cast<interface::Notifier*>(controller.get());
  }

  //! Getter for Event Dispatcher (necessary as inner variable is an weak_ptr)
  auto GetEventDispatcher() -> EventDispatcherMock* {
    auto dummy = controller->dispatcher_.lock();
    return reinterpret_cast<EventDispatcherMock*>(dummy.get());
  }

  //! Getter for Audio Player (necessary as inner variable is an weak_ptr)
  auto GetAudioControl() -> AudioControlMock* {
    auto dummy = controller->player_ctl_.lock();
    return reinterpret_cast<AudioControlMock*>(dummy.get());
  }

  //! Getter for Analyzer (necessary as inner variable is an unique_ptr)
  auto GetAnalyzer() -> AnalyzerMock* {
    return reinterpret_cast<AnalyzerMock*>(controller->analyzer_.get());
  }

  //! Run analysis on raw audio (analyzer output at maximum height) and check bars sent to UI, which
  //! must be at the beginning of fade-in animation only if a new song has started
  void CheckFadeIn(bool new_song) {
    constexpr int kSampleSize = 16;

    auto analysis = [&](TestSyncer& syncer) {
      auto analyzer = GetAnalyzer();
      auto dispatcher = GetEventDispatcher();

      EXPECT_CALL(*analyzer, GetBufferSize()).WillRepeatedly(Return(kSampleSize));
      EXPECT_CALL(*analyzer, GetOutputSize()).WillRepeatedly(Return(kNumberBars));

      EXPECT_CALL(*analyzer, Execute(_, Eq(kSampleSize), _))
          .WillOnce(Invoke([&](double*, int, double* out) {
            std::fill(out, out + kNumberBars, 1.0);
            return error::kSuccess;
          }));

      EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                               interface::CustomEvent::Identifier::UpdateSongInfo)))
          .Times(new_song ? 1 : 0);

      // Right after a new song starts, bars are still at the beginning of fade-in animation
      const Matcher<const std::vector<double>&> expected =
          new_song ? Matcher<const std::vector<double>&>(Each(Lt(0.1)))
                   : Matcher<const std::vector<double>&>(Each(DoubleEq(1.0)));

      EXPECT_CALL(*dispatcher,
                  SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                        interface::CustomEvent::Identifier::DrawAudioSpectrum),
                                  Field(&interface::CustomEvent::content,
                                        VariantWith<std::vector<double>>(expected)))))
          .WillOnce(Invoke([&](const interface::CustomEvent&) { syncer.NotifyStep(2); }));

      syncer.NotifyStep(1);
      RunAnalysisLoop();
    };

    auto client = [&](TestSyncer& syncer) {
      auto notifier = GetInterfaceNotifier();

      syncer.WaitForStep(1);
      if (new_song) {
        notifier->NotifySongInformation(model::Song{.filepath = "song.mp3"});
      }

      std::vector<int16_t> buffer(kSampleSize, 1);
      notifier->SendAudioRaw(buffer.data(), static_cast<int>(buffer.size()));

      syncer.WaitForStep(2);
      controller->Exit();
    };

    testing::RunAsyncTest({analysis, client});
  }

  //! Run analysis loop (same one executed as a thread in the real-life)
  void RunAnalysisLoop() { controller->AnalysisHandler(); }

 protected:
  EventDispatcher dispatcher;  //!< Base class for terminal (graphical interface)
  AudioControl audio_ctl;      //!< Base class for audio player
  MediaController controller;  //!< Middleware between audio player and graphical interface

  static constexpr int kNumberBars = 8;  //!< Default number of bars
};

/* ********************************************************************************************** */

class MediaControllerTestThread : public MediaControllerTest {
 protected:
  void SetUp() override { Init(true); }
};

TEST_F(MediaControllerTestThread, CreateDummyController) {
  // Dummy testing to check setup expectation, and then, exit
  controller->Exit();
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, ExecuteAllMethodsFromAudioNotifier) {
  using ::testing::TypedEq;
  auto notifier = GetPlayerNotifier();
  auto audio_ctl = GetAudioControl();
  auto analyzer = GetAnalyzer();

  InSequence seq;

  std::filesystem::path music{"/stairway/to/heaven.flac"};
  EXPECT_CALL(*audio_ctl, Play(TypedEq<const std::filesystem::path&>(music)));
  notifier->NotifyFileSelection(music);

  EXPECT_CALL(*audio_ctl, PauseOrResume());
  notifier->Pause();

  EXPECT_CALL(*audio_ctl, PauseOrResume());
  notifier->Resume(false);

  EXPECT_CALL(*audio_ctl, PauseOrResume()).Times(0);
  notifier->Resume(true);

  EXPECT_CALL(*audio_ctl, Stop());
  notifier->Stop();

  model::Volume volume{0.7};
  EXPECT_CALL(*audio_ctl, SetAudioVolume(Eq(volume)));
  notifier->SetVolume(volume);

  int number_bars = 16;
  EXPECT_CALL(*analyzer, Init(Eq(number_bars)));
  notifier->ResizeAnalysisOutput(number_bars);

  int skip_seconds = 25;
  EXPECT_CALL(*audio_ctl, SeekForwardPosition(Eq(skip_seconds)));
  notifier->SeekForwardPosition(skip_seconds);

  EXPECT_CALL(*audio_ctl, SeekBackwardPosition(Eq(skip_seconds)));
  notifier->SeekBackwardPosition(skip_seconds);

  model::EqualizerPreset preset = model::AudioFilter::CreatePresets()["Custom"];
  EXPECT_CALL(*audio_ctl, ApplyAudioFilters(preset));
  notifier->ApplyAudioFilters(preset);

  model::Playlist playlist = model::Playlist{
      .name = "Summer Eletrohits Vol. 1",
      .songs = {model::Song{.artist = "Kasino", .title = "Can't get over"}},
  };
  EXPECT_CALL(*audio_ctl, Play(TypedEq<const model::Playlist&>(playlist)));
  notifier->NotifyPlaylistSelection(playlist);

  std::string device{"front:CARD=DAC,DEV=0"};
  EXPECT_CALL(*audio_ctl, SetAudioDevice(device));
  notifier->SetAudioDevice(device);

  model::AudioDevices devices{{.name = device, .description = "USB Audio"}};
  EXPECT_CALL(*audio_ctl, GetAudioDevices()).WillOnce(Return(devices));
  EXPECT_THAT(notifier->GetAudioDevices(), Eq(devices));
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, ExecuteAllMethodsFromInterfaceNotifier) {
  auto notifier = GetInterfaceNotifier();
  auto dispatcher = GetEventDispatcher();

  InSequence seq;

  bool playing = false;
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::ClearSongInfo)));
  notifier->ClearSongInformation(playing);

  model::Song audio{
      .filepath = "/some/custom/path/to/song.mp3",
      .artist = "NIKITO",
      .title = "Bounce",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 123,
  };
  EXPECT_CALL(
      *dispatcher,
      SendEvent(AllOf(
          Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::UpdateSongInfo),
          Field(&interface::CustomEvent::content, VariantWith<model::Song>(audio)))));
  notifier->NotifySongInformation(audio);

  model::Song::CurrentInformation info{.state = model::Song::MediaState::Play, .position = 0};
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::UpdateSongState),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::Song::CurrentInformation>(info)))));
  notifier->NotifySongState(info);

  // Raw audio is only buffered for the analysis thread (not running here), so nothing must be sent
  // to UI. Analysis itself is covered by the tests running the analysis loop
  std::vector<int16_t> buffer(16, 1);
  notifier->SendAudioRaw(buffer.data(), static_cast<int>(buffer.size()));

  error::Code error = error::kUnknownError;
  EXPECT_CALL(*dispatcher, SetApplicationError(Eq(error), StrEq("song.mp3")));
  notifier->NotifyError(error, "song.mp3");

  // Audio output is sent to UI and kept in player status, until song is cleared
  model::AudioOutput output{
      .device = "front:CARD=DAC,DEV=0",
      .format = model::AudioFormat{.sample_rate = 96000, .sample_format = model::SampleFormat::S32},
  };
  EXPECT_CALL(
      *dispatcher,
      SendEvent(AllOf(
          Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::UpdateAudioOutput),
          Field(&interface::CustomEvent::content, VariantWith<model::AudioOutput>(output)))));
  notifier->NotifyAudioOutput(output);
  EXPECT_EQ(controller->GetStatus().output, output);

  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::ClearSongInfo)));
  notifier->ClearSongInformation(playing);
  EXPECT_FALSE(controller->GetStatus().output.has_value());
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, KeepPlayerStatusFromNotifications) {
  using model::Song;
  using ::testing::AnyNumber;

  auto player_notifier = GetPlayerNotifier();
  auto interface_notifier = GetInterfaceNotifier();

  EXPECT_CALL(*GetEventDispatcher(), SendEvent(_)).Times(AnyNumber());
  EXPECT_CALL(*GetAudioControl(), SetAudioVolume(_)).Times(AnyNumber());
  EXPECT_CALL(*GetAudioControl(), SetRepeatMode(_)).Times(AnyNumber());
  EXPECT_CALL(*GetAudioControl(), SetShuffle(_)).Times(AnyNumber());

  // Nothing is playing yet
  model::PlayerStatus status = controller->GetStatus();
  EXPECT_EQ(status.state, Song::MediaState::Empty);
  EXPECT_THAT(status.artist, StrEq(""));
  EXPECT_THAT(status.title, StrEq(""));
  EXPECT_EQ(status.volume, model::Volume{});
  EXPECT_EQ(status.repeat, model::RepeatMode::Off);
  EXPECT_FALSE(status.shuffle);

  // Settings changed by user
  model::Volume volume{0.35F};
  volume.ToggleMute();

  player_notifier->SetVolume(volume);
  player_notifier->SetRepeatMode(model::RepeatMode::One);
  player_notifier->SetShuffle(true);

  status = controller->GetStatus();
  EXPECT_EQ(status.volume, volume);
  EXPECT_TRUE(status.volume.IsMuted());
  EXPECT_EQ(status.repeat, model::RepeatMode::One);
  EXPECT_TRUE(status.shuffle);

  // Song loaded by player, and its state changing while playing
  interface_notifier->NotifySongInformation(Song{
      .filepath = "/path/to/song.mp3", .artist = "NIKITO", .title = "Bounce", .duration = 123});

  status = controller->GetStatus();
  EXPECT_EQ(status.state, Song::MediaState::Play);
  EXPECT_THAT(status.artist, StrEq("NIKITO"));
  EXPECT_THAT(status.title, StrEq("Bounce"));
  EXPECT_EQ(status.position, 0);
  EXPECT_EQ(status.duration, 123);

  interface_notifier->NotifySongState({.state = Song::MediaState::Pause, .position = 42});

  status = controller->GetStatus();
  EXPECT_EQ(status.state, Song::MediaState::Pause);
  EXPECT_EQ(status.position, 42);
  EXPECT_THAT(status.title, StrEq("Bounce"));

  // Song is gone, but settings are kept
  interface_notifier->ClearSongInformation(false);

  status = controller->GetStatus();
  EXPECT_EQ(status.state, Song::MediaState::Empty);
  EXPECT_THAT(status.artist, StrEq(""));
  EXPECT_THAT(status.title, StrEq(""));
  EXPECT_EQ(status.position, 0);
  EXPECT_EQ(status.duration, 0);
  EXPECT_EQ(status.volume, volume);
  EXPECT_EQ(status.repeat, model::RepeatMode::One);
  EXPECT_TRUE(status.shuffle);

  // Without a title in metadata, filename (or URL, for streaming) is used instead
  interface_notifier->NotifySongInformation(Song{.filepath = "/path/to/song.mp3"});
  EXPECT_THAT(controller->GetStatus().title, StrEq("song.mp3"));

  interface_notifier->NotifySongInformation(
      Song{.stream_info = model::StreamInfo{.base_url = "https://www.youtube.com/watch?v=abc"}});
  EXPECT_THAT(controller->GetStatus().title, StrEq("https://www.youtube.com/watch?v=abc"));
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, NotifyPlayerStatusToListener) {
  using model::Song;
  using ::testing::AnyNumber;

  EXPECT_CALL(*GetEventDispatcher(), SendEvent(_)).Times(AnyNumber());
  EXPECT_CALL(*GetAudioControl(), SetShuffle(_)).Times(AnyNumber());

  std::vector<model::PlayerStatus> received;
  controller->SetStatusListener(
      [&received](const model::PlayerStatus& status) { received.push_back(status); });

  // Current status is received right away
  ASSERT_EQ(received.size(), 1);
  EXPECT_EQ(received.back().state, Song::MediaState::Empty);
  EXPECT_FALSE(received.back().shuffle);

  // And then every change, from both UI and player
  GetPlayerNotifier()->SetShuffle(true);
  ASSERT_EQ(received.size(), 2);
  EXPECT_TRUE(received.back().shuffle);

  GetInterfaceNotifier()->NotifySongState({.state = Song::MediaState::Play, .position = 7});
  ASSERT_EQ(received.size(), 3);
  EXPECT_EQ(received.back().state, Song::MediaState::Play);
  EXPECT_EQ(received.back().position, 7);
  EXPECT_TRUE(received.back().shuffle);

  // Until listener is removed
  controller->SetStatusListener(nullptr);
  GetPlayerNotifier()->SetShuffle(false);
  EXPECT_EQ(received.size(), 3);
  EXPECT_FALSE(controller->GetStatus().shuffle);
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, DiscardCommandsWhenPlayerIsGone) {
  auto notifier = GetPlayerNotifier();

  // Simulate application exiting: audio player is destroyed before media controller
  audio_ctl.reset();
  ASSERT_EQ(GetAudioControl(), nullptr);

  // Commands must be discarded (and logged), without crashing
  notifier->NotifyFileSelection("/some/song.mp3");
  notifier->Pause();
  notifier->Resume(/*run_animation=*/false);
  notifier->Stop();
  notifier->SetVolume(model::Volume{0.5f});
  notifier->SeekForwardPosition(1);
  notifier->SeekBackwardPosition(1);
  notifier->ApplyAudioFilters(model::AudioFilter::CreatePresets()["Custom"]);
  notifier->NotifyPlaylistSelection(model::Playlist{.index = 0, .name = "Mix"});
  notifier->NotifyErrorDialogClosed();
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, AnalysisOnRawAudio) {
  int sample_size = 16;

  auto analysis = [&](TestSyncer& syncer) {
    auto analyzer = GetAnalyzer();
    auto dispatcher = GetEventDispatcher();

    // Setup all expectations
    InSequence seq;

    EXPECT_CALL(*analyzer, GetBufferSize()).WillOnce(Return(sample_size));
    EXPECT_CALL(*analyzer, GetOutputSize()).WillOnce(Return(kNumberBars));

    // Thread received a new command, create expectation to analyze and send its result back to UI
    EXPECT_CALL(*analyzer, Execute(_, Eq(sample_size), _))
        .WillOnce(Invoke([&](double*, int, double*) {
          syncer.NotifyStep(2);
          return error::kSuccess;
        }));

    EXPECT_CALL(*dispatcher,
                SendEvent(AllOf(
                    Field(&interface::CustomEvent::id,
                          interface::CustomEvent::Identifier::DrawAudioSpectrum),
                    Field(&interface::CustomEvent::content, VariantWith<std::vector<double>>(_)))));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAnalysisLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto notifier = GetInterfaceNotifier();

    // Send random data to the thread to analyze it
    syncer.WaitForStep(1);
    std::vector<int16_t> buffer(sample_size, 1);
    notifier->SendAudioRaw(buffer.data(), buffer.size());

    // Wait for Analysis to finish before exiting from controller
    syncer.WaitForStep(2);
    controller->Exit();
  };

  testing::RunAsyncTest({analysis, client});
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, FadeInBarsWhenNewSongStarts) { CheckFadeIn(true); }

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, NoFadeInWithoutNewSong) { CheckFadeIn(false); }

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, AnalysisAndClearAnimation) {
  int sample_size = 16;

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Pause,
      .position = 12,
  };

  auto analysis = [&](TestSyncer& syncer) {
    auto analyzer = GetAnalyzer();
    auto dispatcher = GetEventDispatcher();

    EXPECT_CALL(*analyzer, GetBufferSize()).WillRepeatedly(Return(sample_size));
    EXPECT_CALL(*analyzer, GetOutputSize()).WillRepeatedly(Return(kNumberBars));

    std::vector<double> result(kNumberBars, 1);

    {
      // To better readability, split into two scopes to treat each thread command separately
      InSequence seq;

      // Create expectation to analyze data and send its result back to UI
      EXPECT_CALL(*analyzer, Execute(_, Eq(sample_size), _))
          .WillOnce(Invoke([&](double* input, int size, double* output) {
            // Just copy input to output
            std::copy(input, input + kNumberBars, output);
            return error::kSuccess;
          }));

      EXPECT_CALL(
          *dispatcher,
          SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                interface::CustomEvent::Identifier::DrawAudioSpectrum),
                          Field(&interface::CustomEvent::content,
                                VariantWith<std::vector<double>>(ElementsAreArray(result))))))
          .WillOnce(Invoke([&](const interface::CustomEvent&) { syncer.NotifyStep(2); }));
    }

    {
      // Create expectation to execute Clear Animation and send it to UI
      EXPECT_CALL(*dispatcher,
                  SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                        interface::CustomEvent::Identifier::UpdateSongState),
                                  Field(&interface::CustomEvent::content,
                                        VariantWith<model::Song::CurrentInformation>(info)))));

      // This sequence is placed after UpdateSongState event because this specific event is fired
      // from Player thread and not from Analysis thread (in the "real life")
      InSequence seq;

      // As we can get a lot of DrawAudioSpectrum events, calculate result and create expectations
      // Each loop will reduce its previous value by 35%
      for (int i = 0; i < 80; i++) {
        std::transform(result.begin(), result.end(), result.begin(), [](double x) {
          double value = x * 0.75;
          return value > 0.001 ? value : 0.001;
        });

        EXPECT_CALL(
            *dispatcher,
            SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                  interface::CustomEvent::Identifier::DrawAudioSpectrum),
                            Field(&interface::CustomEvent::content,
                                  VariantWith<std::vector<double>>(ElementsAreArray(result))))));
      }

      // Last update from thread with zeroed values for UI
      std::vector<double> last_update(kNumberBars, 0.001);
      EXPECT_CALL(
          *dispatcher,
          SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                interface::CustomEvent::Identifier::DrawAudioSpectrum),
                          Field(&interface::CustomEvent::content,
                                VariantWith<std::vector<double>>(ElementsAreArray(last_update))))))
          .WillOnce(Invoke([&]() { syncer.NotifyStep(3); }));
    }

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAnalysisLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto notifier = GetInterfaceNotifier();

    // In order to run ClearAnimation, must send some raw data first (to fill internal buffer)
    syncer.WaitForStep(1);
    std::vector<int16_t> buffer(sample_size, 1);
    notifier->SendAudioRaw(buffer.data(), buffer.size());

    // Send a Pause notification to run ClearAnimation
    syncer.WaitForStep(2);
    notifier->NotifySongState(info);

    // Wait for Analysis to finish before exiting from controller
    syncer.WaitForStep(3);
    controller->Exit();
  };

  testing::RunAsyncTest({analysis, client});
}

TEST_F(MediaControllerTest, AnalysisAndRegainAnimation) {
  int sample_size = 16;

  auto analysis = [&](TestSyncer& syncer) {
    auto analyzer = GetAnalyzer();
    auto dispatcher = GetEventDispatcher();

    EXPECT_CALL(*analyzer, GetBufferSize()).WillRepeatedly(Return(sample_size));
    EXPECT_CALL(*analyzer, GetOutputSize()).WillRepeatedly(Return(kNumberBars));

    // Player must only be resumed after animation, when UI sends ResumeSong back without animation
    EXPECT_CALL(*GetAudioControl(), PauseOrResume()).Times(0);

    std::vector<double> result(kNumberBars, 1);

    InSequence seq;

    // Create expectation to analyze data and send its result back to UI
    EXPECT_CALL(*analyzer, Execute(_, Eq(sample_size), _))
        .WillOnce(Invoke([&](double* input, int size, double* output) {
          // Just copy input to output
          std::copy(input, input + kNumberBars, output);
          return error::kSuccess;
        }));

    EXPECT_CALL(*dispatcher,
                SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                      interface::CustomEvent::Identifier::DrawAudioSpectrum),
                                Field(&interface::CustomEvent::content,
                                      VariantWith<std::vector<double>>(ElementsAreArray(result))))))
        .WillOnce(Invoke([&](const interface::CustomEvent&) { syncer.NotifyStep(2); }));

    // Each step of Regain Animation increases bars by 1/20 of last analyzed values
    constexpr int kSteps = 20;
    for (double i = 1; i <= kSteps; i++) {
      std::vector<double> bars;
      for (const auto& value : result) bars.push_back(value * (i / kSteps));

      EXPECT_CALL(
          *dispatcher,
          SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                interface::CustomEvent::Identifier::DrawAudioSpectrum),
                          Field(&interface::CustomEvent::content,
                                VariantWith<std::vector<double>>(ElementsAreArray(bars))))));
    }

    // After animation, ask UI to resume song (without running animation again)
    EXPECT_CALL(*dispatcher,
                SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                      interface::CustomEvent::Identifier::ResumeSong),
                                Field(&interface::CustomEvent::content, VariantWith<bool>(false)))))
        .WillOnce(Invoke([&](const interface::CustomEvent&) { syncer.NotifyStep(3); }));

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAnalysisLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto notifier = GetInterfaceNotifier();

    // Regain Animation is based on last analyzed data, so send some raw data first
    syncer.WaitForStep(1);
    std::vector<int16_t> buffer(sample_size, 1);
    notifier->SendAudioRaw(buffer.data(), static_cast<int>(buffer.size()));

    // Ask to resume song with animation
    syncer.WaitForStep(2);
    GetPlayerNotifier()->Resume(/*run_animation=*/true);

    // Wait for Analysis to finish before exiting from controller
    syncer.WaitForStep(3);
    controller->Exit();
  };

  testing::RunAsyncTest({analysis, client});
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

TEST(CustomEventTest, CreateEvents) {
  using interface::CustomEvent;
  using Identifier = CustomEvent::Identifier;
  using Type = CustomEvent::Type;

  const model::Song song{.filepath = "/some/path/to/song.mp3"};
  const model::Playlist playlist{.index = 3, .name = "coding", .songs = {song}};

  // Each event must be created with the type matching its direction, and print its own name
  const std::vector<std::tuple<CustomEvent, Type, Identifier, std::string>> events{
      // From audio thread to interface
      {CustomEvent::ClearSongInfo(), Type::FromAudioThreadToInterface, Identifier::ClearSongInfo,
       "ClearSongInfo"},
      {CustomEvent::UpdateVolume(model::Volume{}), Type::FromAudioThreadToInterface,
       Identifier::UpdateVolume, "UpdateVolume"},
      {CustomEvent::UpdateSongInfo(song), Type::FromAudioThreadToInterface,
       Identifier::UpdateSongInfo, "UpdateSongInfo"},
      {CustomEvent::UpdateSongState({}), Type::FromAudioThreadToInterface,
       Identifier::UpdateSongState, "UpdateSongState"},
      {CustomEvent::DrawAudioSpectrum({}), Type::FromAudioThreadToInterface,
       Identifier::DrawAudioSpectrum, "DrawAudioSpectrum"},

      // From interface to audio thread
      {CustomEvent::NotifyFileSelection(song.filepath), Type::FromInterfaceToAudioThread,
       Identifier::NotifyFileSelection, "NotifyFileSelection"},
      {CustomEvent::PauseSong(), Type::FromInterfaceToAudioThread, Identifier::PauseSong,
       "PauseSong"},
      {CustomEvent::ResumeSong(true), Type::FromInterfaceToAudioThread, Identifier::ResumeSong,
       "ResumeSong"},
      {CustomEvent::StopSong(), Type::FromInterfaceToAudioThread, Identifier::StopSong, "StopSong"},
      {CustomEvent::SetAudioVolume(model::Volume{}), Type::FromInterfaceToAudioThread,
       Identifier::SetAudioVolume, "SetAudioVolume"},
      {CustomEvent::ResizeAnalysis(16), Type::FromInterfaceToAudioThread,
       Identifier::ResizeAnalysis, "ResizeAnalysis"},
      {CustomEvent::SeekForwardPosition(1), Type::FromInterfaceToAudioThread,
       Identifier::SeekForwardPosition, "SeekForwardPosition"},
      {CustomEvent::SeekBackwardPosition(1), Type::FromInterfaceToAudioThread,
       Identifier::SeekBackwardPosition, "SeekBackwardPosition"},
      {CustomEvent::ApplyAudioFilters({}), Type::FromInterfaceToAudioThread,
       Identifier::ApplyAudioFilters, "ApplyAudioFilters"},
      {CustomEvent::NotifyPlaylistSelection(playlist), Type::FromInterfaceToAudioThread,
       Identifier::NotifyPlaylistSelection, "NotifyPlaylistSelection"},
      {CustomEvent::NotifyDialogClosed(), Type::FromInterfaceToAudioThread,
       Identifier::NotifyDialogClosed, "NotifyDialogClosed"},
      {CustomEvent::SkipToNextPlaylistSong(), Type::FromInterfaceToAudioThread,
       Identifier::SkipToNextPlaylistSong, "SkipToNextPlaylistSong"},
      {CustomEvent::SkipToPreviousPlaylistSong(), Type::FromInterfaceToAudioThread,
       Identifier::SkipToPreviousPlaylistSong, "SkipToPreviousPlaylistSong"},
      {CustomEvent::SetRepeatMode(model::RepeatMode::Off), Type::FromInterfaceToAudioThread,
       Identifier::SetRepeatMode, "SetRepeatMode"},
      {CustomEvent::SetShuffle(true), Type::FromInterfaceToAudioThread, Identifier::SetShuffle,
       "SetShuffle"},

      // From interface to interface
      {CustomEvent::Refresh(), Type::FromInterfaceToInterface, Identifier::Refresh, "Refresh"},
      {CustomEvent::EnableGlobalEvent(), Type::FromInterfaceToInterface,
       Identifier::EnableGlobalEvent, "EnableGlobalEvent"},
      {CustomEvent::DisableGlobalEvent(), Type::FromInterfaceToInterface,
       Identifier::DisableGlobalEvent, "DisableGlobalEvent"},
      {CustomEvent::ChangeBarAnimation(model::BarAnimation::Mono), Type::FromInterfaceToInterface,
       Identifier::ChangeBarAnimation, "ChangeBarAnimation"},
      {CustomEvent::ShowHelper(), Type::FromInterfaceToInterface, Identifier::ShowHelper,
       "ShowHelper"},
      {CustomEvent::CalculateNumberOfBars(16), Type::FromInterfaceToInterface,
       Identifier::CalculateNumberOfBars, "CalculateNumberOfBars"},
      {CustomEvent::SetPreviousFocused(), Type::FromInterfaceToInterface,
       Identifier::SetPreviousFocused, "SetPreviousFocused"},
      {CustomEvent::SetNextFocused(), Type::FromInterfaceToInterface, Identifier::SetNextFocused,
       "SetNextFocused"},
      {CustomEvent::SetFocused(model::BlockIdentifier::Sidebar), Type::FromInterfaceToInterface,
       Identifier::SetFocused, "SetFocused"},
      {CustomEvent::PlaySong(), Type::FromInterfaceToInterface, Identifier::PlaySong, "PlaySong"},
      {CustomEvent::ToggleFullscreen(), Type::FromInterfaceToInterface,
       Identifier::ToggleFullscreen, "ToggleFullscreen"},
      {CustomEvent::UpdateBarWidth(), Type::FromInterfaceToInterface, Identifier::UpdateBarWidth,
       "UpdateBarWidth"},
      {CustomEvent::ShowPlaylistManager({}), Type::FromInterfaceToInterface,
       Identifier::ShowPlaylistManager, "ShowPlaylistManager"},
      {CustomEvent::SavePlaylistsToFile(playlist), Type::FromInterfaceToInterface,
       Identifier::SavePlaylistsToFile, "SavePlaylistsToFile"},
      {CustomEvent::ShowQuestionDialog({}), Type::FromInterfaceToInterface,
       Identifier::ShowQuestionDialog, "ShowQuestionDialog"},
      {CustomEvent::Exit(), Type::FromInterfaceToInterface, Identifier::Exit, "Exit"},
      {CustomEvent::ShowWarning("oops"), Type::FromInterfaceToInterface, Identifier::ShowWarning,
       "ShowWarning"},
      {CustomEvent::RunRemoteCommand(model::RemoteCommand::Stop), Type::FromInterfaceToInterface,
       Identifier::RunRemoteCommand, "RunRemoteCommand"},
  };

  for (const auto& [event, type, id, name] : events) {
    EXPECT_EQ(event.type, type) << name;
    EXPECT_EQ(event.GetId(), id) << name;
    EXPECT_TRUE(event == id) << name;

    EXPECT_THAT(Print(id), ::testing::StrEq(name));
  }
}

/* ********************************************************************************************** */

TEST(CustomEventTest, GetContent) {
  using interface::CustomEvent;
  auto event = CustomEvent::SeekForwardPosition(7);
  EXPECT_EQ(event.GetContent<int>(), 7);

  // When content does not hold the given type, a default value is returned
  EXPECT_TRUE(event.GetContent<std::string>().empty());
  EXPECT_FALSE(event.GetContent<bool>());

  EXPECT_TRUE(event != CustomEvent::Identifier::SeekBackwardPosition);
}

/* ********************************************************************************************** */

TEST(CustomEventTest, PrintEventWithEachContent) {
  using interface::CustomEvent;
  // Empty
  EXPECT_THAT(Print(CustomEvent::ClearSongInfo()),
              ::testing::StrEq(R"({type:"Player->UI", id:"ClearSongInfo", content:"empty"})"));

  // Integer, boolean and string
  EXPECT_THAT(Print(CustomEvent::ResizeAnalysis(16)),
              ::testing::StrEq(R"({type:"UI->Player", id:"ResizeAnalysis", content:16})"));

  EXPECT_THAT(Print(CustomEvent::SetShuffle(true)),
              ::testing::StrEq(R"({type:"UI->Player", id:"SetShuffle", content:true})"));

  EXPECT_THAT(Print(CustomEvent::ResumeSong(false)),
              ::testing::StrEq(R"({type:"UI->Player", id:"ResumeSong", content:false})"));

  EXPECT_THAT(Print(CustomEvent::ShowWarning("oops")),
              ::testing::StrEq(R"({type:"UI->UI", id:"ShowWarning", content:"oops"})"));

  // Repeat mode
  EXPECT_THAT(Print(CustomEvent::SetRepeatMode(model::RepeatMode::Off)),
              ::testing::StrEq(R"({type:"UI->Player", id:"SetRepeatMode", content:off})"));

  // Song
  const model::Song song{.filepath = "/some/path/to/song.mp3", .artist = "cln", .title = "DUST"};
  EXPECT_THAT(Print(CustomEvent::UpdateSongInfo(song)),
              ::testing::AllOf(
                  ::testing::HasSubstr(R"({type:"Player->UI", id:"UpdateSongInfo", content:{)"),
                  ::testing::HasSubstr(R"(filename:"song.mp3", artist:"cln", title:"DUST")")));

  // Volume
  EXPECT_THAT(
      Print(CustomEvent::UpdateVolume(model::Volume{0.5F})),
      ::testing::StrEq(
          R"({type:"Player->UI", id:"UpdateVolume", content:{volume:"50%", muted:false}})"));

  // Song state
  const model::Song::CurrentInformation info{.state = model::Song::MediaState::Pause,
                                             .position = 12};
  EXPECT_THAT(
      Print(CustomEvent::UpdateSongState(info)),
      ::testing::StrEq(
          R"({type:"Player->UI", id:"UpdateSongState", content:{state:"Pause", position:12}})"));

  // Path
  EXPECT_THAT(
      Print(CustomEvent::NotifyFileSelection(song.filepath)),
      ::testing::StrEq(
          R"({type:"UI->Player", id:"NotifyFileSelection", content:"/some/path/to/song.mp3"})"));

  // Spectrum data and audio filters are not printed, as they are too big
  EXPECT_THAT(Print(CustomEvent::DrawAudioSpectrum({0.1, 0.2})),
              ::testing::StrEq(
                  R"({type:"Player->UI", id:"DrawAudioSpectrum", content:"{vector data...}"})"));

  EXPECT_THAT(
      Print(CustomEvent::ApplyAudioFilters({})),
      ::testing::StrEq(
          R"({type:"UI->Player", id:"ApplyAudioFilters", content:"{audio filter data...}"})"));

  // Bar animation
  EXPECT_THAT(Print(CustomEvent::ChangeBarAnimation(model::BarAnimation::Mono)),
              ::testing::StrEq(R"({type:"UI->UI", id:"ChangeBarAnimation", content:"Mono"})"));

  // Block identifier
  EXPECT_THAT(Print(CustomEvent::SetFocused(model::BlockIdentifier::MediaPlayer)),
              ::testing::StrEq(R"({type:"UI->UI", id:"SetFocused", content:"MediaPlayer"})"));

  // Playlist
  const model::Playlist playlist{.index = 3, .name = "coding", .songs = {song}};
  EXPECT_THAT(Print(CustomEvent::NotifyPlaylistSelection(playlist)),
              ::testing::StrEq(R"({type:"UI->Player", id:"NotifyPlaylistSelection", )"
                               R"(content:{id:3, playlist:"coding", songs:1}})"));

  // Playlist operation
  const model::PlaylistOperation create{.action = model::PlaylistOperation::Operation::Create};
  EXPECT_THAT(Print(CustomEvent::ShowPlaylistManager(create)),
              ::testing::StrEq(R"({type:"UI->UI", id:"ShowPlaylistManager", )"
                               R"(content:{action:"Create", playlist:{}}})"));

  const model::PlaylistOperation modify{.action = model::PlaylistOperation::Operation::Modify,
                                        .playlist = playlist};
  EXPECT_THAT(Print(CustomEvent::ShowPlaylistManager(modify)),
              ::testing::StrEq(
                  R"({type:"UI->UI", id:"ShowPlaylistManager", )"
                  R"(content:{action:"Modify", playlist:{id:3, playlist:"coding", songs:1}}})"));

  // Question data
  const model::QuestionData question{.question = "Quit?", .cb_yes = [] {}};
  EXPECT_THAT(
      Print(CustomEvent::ShowQuestionDialog(question)),
      ::testing::StrEq(R"({type:"UI->UI", id:"ShowQuestionDialog", )"
                       R"(content:{question: "Quit?", cb_yes:"not empty", cb_no:"empty"}})"));
}

/* ********************************************************************************************** */

/**
 * @brief Mock class for audio notifier API (events sent from interface to audio thread)
 */
class AudioNotifierMock : public audio::Notifier {
 public:
  MOCK_METHOD(void, NotifyFileSelection, (const std::filesystem::path&), (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (bool), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, SetVolume, (model::Volume), (override));
  MOCK_METHOD(void, ResizeAnalysisOutput, (int), (override));
  MOCK_METHOD(void, SeekForwardPosition, (int), (override));
  MOCK_METHOD(void, SeekBackwardPosition, (int), (override));
  MOCK_METHOD(void, ApplyAudioFilters, (const model::EqualizerPreset&), (override));
  MOCK_METHOD(void, NotifyPlaylistSelection, (const model::Playlist&), (override));
  MOCK_METHOD(void, NotifyErrorDialogClosed, (), (override));
  MOCK_METHOD(void, SkipToNextSong, (), (override));
  MOCK_METHOD(void, SkipToPreviousSong, (), (override));
  MOCK_METHOD(void, SetRepeatMode, (model::RepeatMode), (override));
  MOCK_METHOD(void, SetShuffle, (bool), (override));
  MOCK_METHOD(void, SetAudioDevice, (const std::string&), (override));
  MOCK_METHOD(model::AudioDevices, GetAudioDevices, (), (override));
};

/**
 * @brief Tests with Terminal class
 */
class TerminalTest : public ::testing::Test {
 protected:
  using Terminal = interface::Terminal;

  //! Terminal size big enough to render all blocks
  static constexpr int kColumns = 120;
  static constexpr int kLines = 30;

  //! Terminal size smaller than the minimum one
  static constexpr int kSmallColumns = 80;
  static constexpr int kSmallLines = 24;

  //! Index from each block
  static constexpr int kSidebar = Terminal::kBlockSidebar;
  static constexpr int kFileInfo = Terminal::kBlockFileInfo;
  static constexpr int kMainContent = Terminal::kBlockMainContent;
  static constexpr int kMediaPlayer = Terminal::kBlockMediaPlayer;

  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override { CreateTerminal(); }

  void TearDown() override {
    terminal.reset();
    notifier.reset();
  }

  //! Create terminal with a fixed size (using a mock to not load/save files from user's home)
  void CreateTerminal() {
    terminal = Terminal::Create(LISTDIR_PATH, file_handler);

    size = ftxui::Dimensions{kColumns, kLines};
    terminal->cb_size_ = [this] { return size; };
    terminal->size_ = size;

    terminal->RegisterEventSenderCallback([this](const ftxui::Event&) { ++refresh_count; });
    terminal->RegisterExitCallback([this] { ++exit_count; });
  }

  //! Register audio notifier, as middleware does when application starts
  void RegisterNotifier() { terminal->RegisterPlayerNotifier(notifier); }

  //! Simulate user resizing the terminal (noticed by Terminal on next render)
  void Resize(int columns, int lines) { size = ftxui::Dimensions{columns, lines}; }

  //! Render terminal and get it as text
  std::string Render() {
    auto element = terminal->Render();

    ftxui::Screen screen(size.dimx, size.dimy);
    ftxui::Render(screen, element);

    return utils::FilterAnsiCommands(screen.ToString());
  }

  //! Send a keyboard event, which also makes terminal handle any pending custom event
  bool Send(const ftxui::Event& event) { return terminal->OnEvent(event); }

  //! Make terminal handle any pending custom event
  void HandlePendingEvents() { Send(ftxui::Event::Custom); }

  //! Getters for internal state
  int GetFocusedIndex() const { return terminal->focused_index_; }
  bool IsBlockFocused(int index) const {
    return std::static_pointer_cast<interface::Block>(terminal->children_.at(index))->IsFocused();
  }

  bool IsFullscreen() const { return terminal->fullscreen_mode_; }
  bool IsErrorVisible() const { return terminal->error_dialog_->IsVisible(); }
  bool IsHelpVisible() const { return terminal->help_dialog_->IsVisible(); }
  bool IsQuestionVisible() const { return terminal->question_dialog_->IsVisible(); }
  bool IsPlaylistDialogVisible() const { return terminal->playlist_dialog_->IsVisible(); }
  bool IsThemePickerVisible() const { return terminal->theme_picker_->IsVisible(); }
  bool IsDevicePickerVisible() const { return terminal->device_picker_->IsVisible(); }

  utils::ThemeGuard guard;  //!< Restore default theme when test finishes

  //! Load/save settings (by default, there are no settings saved)
  std::shared_ptr<::testing::NiceMock<FileHandlerMock>> file_handler =
      std::make_shared<::testing::NiceMock<FileHandlerMock>>();

  std::shared_ptr<::testing::NiceMock<AudioNotifierMock>> notifier =
      std::make_shared<::testing::NiceMock<AudioNotifierMock>>();

  std::shared_ptr<Terminal> terminal;
  ftxui::Dimensions size;  //!< Size reported to terminal

  int refresh_count = 0;  //!< Number of times that terminal asked to refresh screen
  int exit_count = 0;     //!< Number of times that terminal asked to exit application
};

/* ********************************************************************************************** */

TEST_F(TerminalTest, RenderAllBlocks) {
  const std::string rendered = Render();

  EXPECT_THAT(rendered, ::testing::HasSubstr("F1:files"));
  EXPECT_THAT(rendered, ::testing::HasSubstr(" information "));
  EXPECT_THAT(rendered, ::testing::HasSubstr(" player "));
  EXPECT_THAT(rendered, ::testing::Not(::testing::HasSubstr("Terminal too small")));

  // Sidebar is the one focused when application starts
  EXPECT_EQ(GetFocusedIndex(), kSidebar);
  EXPECT_TRUE(IsBlockFocused(kSidebar));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, RenderTooSmall) {
  Resize(kSmallColumns, kSmallLines);
  const std::string rendered = Render();

  EXPECT_THAT(rendered, ::testing::HasSubstr("Terminal too small"));
  EXPECT_THAT(rendered, ::testing::HasSubstr("Current: 80x24"));
  EXPECT_THAT(rendered, ::testing::HasSubstr("Minimum: 105x24"));
  EXPECT_THAT(rendered, ::testing::HasSubstr("Resize it or press q to quit"));
  EXPECT_THAT(rendered, ::testing::Not(::testing::HasSubstr(" player ")));

  // Blocks are not visible, so user cannot interact with them
  EXPECT_FALSE(Send(interface::keybinding::Navigation::Tab));
  EXPECT_EQ(GetFocusedIndex(), kSidebar);

  EXPECT_FALSE(Send(interface::keybinding::General::ShowHelper));
  EXPECT_FALSE(IsHelpVisible());

  // Only quit is allowed
  EXPECT_EQ(exit_count, 0);
  EXPECT_TRUE(Send(interface::keybinding::General::ExitApplication));
  EXPECT_EQ(exit_count, 1);

  // Blocks are rendered again after resizing terminal
  Resize(kColumns, kLines);
  EXPECT_THAT(Render(), ::testing::HasSubstr(" player "));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ResizeRecalculatesNumberOfBars) {
  Render();
  const int bars = terminal->CalculateNumberBars();
  const int refresh = refresh_count;

  EXPECT_GT(bars, 0);
  EXPECT_EQ(bars % 2, 0);

  // Nothing changes without a resize
  Render();
  EXPECT_EQ(refresh_count, refresh);

  // A wider terminal fits more bars, and spectrum visualizer must be informed about it
  Resize(kColumns * 2, kLines);
  Render();

  EXPECT_GT(refresh_count, refresh);
  EXPECT_GT(terminal->CalculateNumberBars(), bars);
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, NumberOfBarsDependsOnAnimation) {
  using interface::CustomEvent;

  terminal->ProcessEvent(CustomEvent::ChangeBarAnimation(model::BarAnimation::HorizontalMirror));
  const int spaced = terminal->CalculateNumberBars();

  // Without space between bars, more of them fit in the same width
  terminal->ProcessEvent(
      CustomEvent::ChangeBarAnimation(model::BarAnimation::HorizontalMirrorNoSpace));
  const int not_spaced = terminal->CalculateNumberBars();

  EXPECT_GT(not_spaced, spaced);

  // Animation is kept when only bar width changes
  terminal->ProcessEvent(CustomEvent::UpdateBarWidth());
  EXPECT_EQ(terminal->CalculateNumberBars(), not_spaced);

  // Given by parameter, it replaces the last one
  EXPECT_EQ(terminal->CalculateNumberBars(model::BarAnimation::HorizontalMirror), spaced);
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ExitApplication) {
  EXPECT_TRUE(Send(interface::keybinding::General::ExitApplication));
  EXPECT_EQ(exit_count, 1);

  // Any block may also ask to exit
  terminal->ProcessEvent(interface::CustomEvent::Exit());
  EXPECT_EQ(exit_count, 2);
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, DisableGlobalEvents) {
  using interface::CustomEvent;

  // Blocks disable global events while user is typing (e.g. searching in a list)
  terminal->ProcessEvent(CustomEvent::DisableGlobalEvent());

  Send(interface::keybinding::General::ExitApplication);
  Send(interface::keybinding::General::ShowHelper);

  EXPECT_EQ(exit_count, 0);
  EXPECT_FALSE(IsHelpVisible());

  terminal->ProcessEvent(CustomEvent::EnableGlobalEvent());

  EXPECT_TRUE(Send(interface::keybinding::General::ExitApplication));
  EXPECT_EQ(exit_count, 1);
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, SwitchFocusWithKeys) {
  using interface::keybinding::General;
  using interface::keybinding::Navigation;

  // Focus next block, returning to the first one after the last
  for (int index : {kFileInfo, kMainContent, kMediaPlayer, kSidebar}) {
    EXPECT_TRUE(Send(Navigation::Tab));
    EXPECT_EQ(GetFocusedIndex(), index);
    EXPECT_TRUE(IsBlockFocused(index));
  }

  // Focus previous block, going to the last one from the first
  for (int index : {kMediaPlayer, kMainContent, kFileInfo, kSidebar}) {
    EXPECT_TRUE(Send(Navigation::TabReverse));
    EXPECT_EQ(GetFocusedIndex(), index);
    EXPECT_TRUE(IsBlockFocused(index));
  }

  EXPECT_FALSE(IsBlockFocused(kMediaPlayer));

  // Focus a specific block
  EXPECT_TRUE(Send(General::FocusPlayer));
  EXPECT_EQ(GetFocusedIndex(), kMediaPlayer);

  EXPECT_TRUE(Send(General::FocusMainContent));
  EXPECT_EQ(GetFocusedIndex(), kMainContent);

  EXPECT_TRUE(Send(General::FocusInfo));
  EXPECT_EQ(GetFocusedIndex(), kFileInfo);

  EXPECT_TRUE(Send(General::FocusSidebar));
  EXPECT_EQ(GetFocusedIndex(), kSidebar);

  EXPECT_TRUE(IsBlockFocused(kSidebar));
  EXPECT_FALSE(IsBlockFocused(kFileInfo));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, SwitchFocusWithEvents) {
  using interface::CustomEvent;

  terminal->ProcessEvent(CustomEvent::SetFocused(model::BlockIdentifier::MediaPlayer));
  EXPECT_EQ(GetFocusedIndex(), kMediaPlayer);
  EXPECT_TRUE(IsBlockFocused(kMediaPlayer));
  EXPECT_FALSE(IsBlockFocused(kSidebar));

  terminal->ProcessEvent(CustomEvent::SetFocused(model::BlockIdentifier::FileInfo));
  EXPECT_EQ(GetFocusedIndex(), kFileInfo);

  terminal->ProcessEvent(CustomEvent::SetFocused(model::BlockIdentifier::MainContent));
  EXPECT_EQ(GetFocusedIndex(), kMainContent);

  terminal->ProcessEvent(CustomEvent::SetNextFocused());
  EXPECT_EQ(GetFocusedIndex(), kMediaPlayer);

  terminal->ProcessEvent(CustomEvent::SetPreviousFocused());
  EXPECT_EQ(GetFocusedIndex(), kMainContent);

  // An unknown block falls back to the first one
  terminal->ProcessEvent(CustomEvent::SetFocused(model::BlockIdentifier::None));
  EXPECT_EQ(GetFocusedIndex(), kSidebar);
  EXPECT_TRUE(IsBlockFocused(kSidebar));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, SendEventsToAudioThread) {
  using interface::CustomEvent;
  using ::testing::_;

  RegisterNotifier();
  HandlePendingEvents();

  const std::filesystem::path file{"/some/path/to/song.mp3"};
  const model::Playlist playlist{.index = 3, .name = "coding", .songs = {model::Song{}}};
  const model::Volume volume{0.4F};
  const int bars = 16;

  EXPECT_CALL(*notifier, NotifyFileSelection(file));
  terminal->ProcessEvent(CustomEvent::NotifyFileSelection(file));

  EXPECT_CALL(*notifier, Pause());
  terminal->ProcessEvent(CustomEvent::PauseSong());

  EXPECT_CALL(*notifier, Resume(true));
  terminal->ProcessEvent(CustomEvent::ResumeSong(true));

  EXPECT_CALL(*notifier, Stop());
  terminal->ProcessEvent(CustomEvent::StopSong());

  EXPECT_CALL(*notifier, SetVolume(volume));
  terminal->ProcessEvent(CustomEvent::SetAudioVolume(volume));

  EXPECT_CALL(*notifier, ResizeAnalysisOutput(bars));
  terminal->ProcessEvent(CustomEvent::ResizeAnalysis(bars));

  EXPECT_CALL(*notifier, SeekForwardPosition(2));
  terminal->ProcessEvent(CustomEvent::SeekForwardPosition(2));

  EXPECT_CALL(*notifier, SeekBackwardPosition(1));
  terminal->ProcessEvent(CustomEvent::SeekBackwardPosition(1));

  EXPECT_CALL(*notifier, ApplyAudioFilters(_));
  terminal->ProcessEvent(CustomEvent::ApplyAudioFilters({}));

  EXPECT_CALL(*notifier, NotifyPlaylistSelection(playlist));
  terminal->ProcessEvent(CustomEvent::NotifyPlaylistSelection(playlist));

  EXPECT_CALL(*notifier, NotifyErrorDialogClosed());
  terminal->ProcessEvent(CustomEvent::NotifyDialogClosed());

  EXPECT_CALL(*notifier, SkipToNextSong());
  terminal->ProcessEvent(CustomEvent::SkipToNextPlaylistSong());

  EXPECT_CALL(*notifier, SkipToPreviousSong());
  terminal->ProcessEvent(CustomEvent::SkipToPreviousPlaylistSong());

  EXPECT_CALL(*notifier, SetRepeatMode(model::RepeatMode::One));
  terminal->ProcessEvent(CustomEvent::SetRepeatMode(model::RepeatMode::One));

  EXPECT_CALL(*notifier, SetShuffle(true));
  terminal->ProcessEvent(CustomEvent::SetShuffle(true));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, KeepEventsToAudioThreadUntilNotifierIsRegistered) {
  using interface::CustomEvent;
  using ::testing::_;

  // Volume from last run is restored by media player while terminal is being created
  ON_CALL(*file_handler, ParseSettings(_))
      .WillByDefault(::testing::DoAll(::testing::SetArgReferee<0>(model::Settings{.volume = 40}),
                                      ::testing::Return(true)));
  CreateTerminal();

  EXPECT_CALL(*notifier, SetVolume(_)).Times(0);
  EXPECT_CALL(*notifier, Pause()).Times(0);

  HandlePendingEvents();
  terminal->ProcessEvent(CustomEvent::PauseSong());

  ::testing::Mock::VerifyAndClearExpectations(notifier.get());

  // Events are sent to audio thread as soon as it can be reached
  EXPECT_CALL(*notifier, SetVolume(model::Volume{0.4F}));
  EXPECT_CALL(*notifier, Pause());
  RegisterNotifier();

  ::testing::Mock::VerifyAndClearExpectations(notifier.get());

  // And they are discarded when audio thread is gone (application is exiting)
  notifier.reset();
  terminal->ProcessEvent(CustomEvent::PauseSong());

  EXPECT_THAT(Render(), ::testing::HasSubstr("Volume:  40%"));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, SendEventsFromAudioThreadToBlocks) {
  using interface::CustomEvent;

  const int refresh = refresh_count;

  // Events are queued, and screen is asked to refresh, so they are handled by UI thread
  terminal->SendEvent(CustomEvent::UpdateSongInfo(model::Song{
      .filepath = "/some/path/to/song.mp3",
      .artist = "cln",
      .title = "DUST",
      .duration = 146,
  }));

  EXPECT_EQ(refresh_count, refresh + 1);
  EXPECT_THAT(Render(), ::testing::Not(::testing::HasSubstr("02:26")));

  HandlePendingEvents();
  EXPECT_THAT(Render(), ::testing::HasSubstr("02:26"));

  // While this one is handled right away
  terminal->ProcessEvent(CustomEvent::ClearSongInfo());
  EXPECT_THAT(Render(), ::testing::Not(::testing::HasSubstr("02:26")));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ShowWarningOnMediaPlayer) {
  terminal->SetApplicationError(error::kInvalidFile, "song.mp3");
  HandlePendingEvents();

  // Warning does not interrupt user
  EXPECT_FALSE(IsErrorVisible());
  EXPECT_THAT(Render(), ::testing::HasSubstr("Invalid file: song.mp3"));

  // Without any detail, only its message is shown
  terminal->SetApplicationError(error::kCorruptedData, "");
  HandlePendingEvents();

  EXPECT_FALSE(IsErrorVisible());
  EXPECT_THAT(Render(), ::testing::HasSubstr("File is corrupted"));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ShowErrorDialog) {
  RegisterNotifier();

  terminal->SetApplicationError(error::kTooManyFailedSongs, "");

  EXPECT_TRUE(IsErrorVisible());
  EXPECT_THAT(Render(), ::testing::HasSubstr("Several songs failed in a row"));

  // Nothing else is handled while dialog is opened
  Send(interface::keybinding::General::ShowHelper);
  Send(interface::keybinding::Navigation::Tab);

  EXPECT_FALSE(IsHelpVisible());
  EXPECT_EQ(GetFocusedIndex(), kSidebar);
  EXPECT_EQ(exit_count, 0);

  // Audio thread is informed when user closes it
  EXPECT_CALL(*notifier, NotifyErrorDialogClosed());

  EXPECT_TRUE(Send(interface::keybinding::Navigation::Escape));
  HandlePendingEvents();

  EXPECT_FALSE(IsErrorVisible());
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ShowHelpForFocusedBlock) {
  using interface::CustomEvent;
  using interface::keybinding::General;
  using interface::keybinding::Navigation;

  // Open help from every block and view (it starts from the section related to it)
  const std::vector<std::vector<ftxui::Event>> steps{
      {},
      {interface::keybinding::Sidebar::FocusPlaylist},
      {General::FocusInfo},
      {General::FocusMainContent},
      {interface::keybinding::MainContent::FocusEqualizer},
      {interface::keybinding::MainContent::FocusLyric},
      {General::FocusPlayer},
  };

  for (const auto& keys : steps) {
    for (const auto& key : keys) Send(key);

    EXPECT_TRUE(Send(General::ShowHelper));
    EXPECT_TRUE(IsHelpVisible());
    EXPECT_THAT(Render(), ::testing::HasSubstr("help"));

    // Keys go to dialog while it is opened, so application does not exit
    EXPECT_TRUE(Send(Navigation::Close));
    EXPECT_FALSE(IsHelpVisible());
    EXPECT_EQ(exit_count, 0);
  }

  EXPECT_EQ(GetFocusedIndex(), kMediaPlayer);

  // Any block may also ask to show it
  terminal->ProcessEvent(CustomEvent::ShowHelper());
  EXPECT_TRUE(IsHelpVisible());
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ShowThemePicker) {
  EXPECT_TRUE(Send(interface::keybinding::General::ChangeTheme));
  EXPECT_TRUE(IsThemePickerVisible());
  EXPECT_THAT(Render(), ::testing::HasSubstr("Tokyo Night"));

  // Keys go to picker while it is opened
  Send(interface::keybinding::Navigation::Tab);
  EXPECT_EQ(GetFocusedIndex(), kSidebar);

  EXPECT_TRUE(Send(interface::keybinding::Navigation::Escape));
  EXPECT_FALSE(IsThemePickerVisible());
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ChooseDeviceWithPicker) {
  using interface::keybinding::General;
  using interface::keybinding::Navigation;
  using ::testing::_;

  const std::string device{"front:CARD=DAC,DEV=0"};

  RegisterNotifier();

  EXPECT_CALL(*notifier, GetAudioDevices())
      .WillOnce(::testing::Return(model::AudioDevices{
          {.name = "default", .description = "Default output"},
          {.name = device, .description = "USB Audio"},
      }));

  EXPECT_TRUE(Send(General::ChangeAudioDevice));
  EXPECT_TRUE(IsDevicePickerVisible());

  // First entry is always the one to not choose any device
  std::string rendered = Render();
  EXPECT_THAT(rendered, ::testing::HasSubstr("▶ automatic"));
  EXPECT_THAT(rendered, ::testing::HasSubstr("default               Default output"));
  EXPECT_THAT(rendered, ::testing::HasSubstr(device + "  USB Audio"));

  // Keys go to picker while it is opened
  Send(Navigation::Tab);
  EXPECT_EQ(GetFocusedIndex(), kSidebar);

  // Nothing is sent to audio thread while selection moves (it does not go beyond last entry)
  EXPECT_CALL(*notifier, SetAudioDevice(_)).Times(0);

  Send(Navigation::ArrowDown);
  Send(Navigation::Down);
  Send(Navigation::Down);
  HandlePendingEvents();
  EXPECT_THAT(Render(), ::testing::HasSubstr("▶ " + device));

  ::testing::Mock::VerifyAndClearExpectations(notifier.get());

  // Chosen device is saved and sent to audio thread
  EXPECT_CALL(*notifier, SetAudioDevice(device));
  EXPECT_CALL(*file_handler, SaveSettings(::testing::Field(&model::Settings::device, device)))
      .WillOnce(::testing::Return(true));

  EXPECT_TRUE(Send(Navigation::Return));
  EXPECT_FALSE(IsDevicePickerVisible());
  HandlePendingEvents();

  ::testing::Mock::VerifyAndClearExpectations(notifier.get());

  // When opened again, device in use is the selected one
  EXPECT_CALL(*notifier, GetAudioDevices())
      .WillOnce(::testing::Return(model::AudioDevices{
          {.name = "default", .description = "Default output"},
          {.name = device, .description = "USB Audio"},
      }));

  Send(General::ChangeAudioDevice);
  EXPECT_THAT(Render(), ::testing::HasSubstr("▶ " + device));

  // Choosing the first entry lets audio thread choose device again
  EXPECT_CALL(*notifier, SetAudioDevice(""));
  EXPECT_CALL(*file_handler, SaveSettings(::testing::Field(&model::Settings::device, "")))
      .WillOnce(::testing::Return(true));

  Send(Navigation::Up);
  Send(Navigation::ArrowUp);
  Send(Navigation::Up);
  EXPECT_TRUE(Send(General::ChangeAudioDevice));
  EXPECT_FALSE(IsDevicePickerVisible());
  HandlePendingEvents();
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, CancelDevicePicker) {
  using interface::keybinding::General;
  using interface::keybinding::Navigation;
  using ::testing::_;

  RegisterNotifier();

  EXPECT_CALL(*notifier, GetAudioDevices())
      .WillOnce(::testing::Return(model::AudioDevices{{.name = "default"}}));

  // Device in use is kept
  EXPECT_CALL(*notifier, SetAudioDevice(_)).Times(0);
  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  EXPECT_TRUE(Send(General::ChangeAudioDevice));
  EXPECT_TRUE(IsDevicePickerVisible());

  Send(Navigation::Down);
  EXPECT_TRUE(Send(Navigation::Escape));
  EXPECT_FALSE(IsDevicePickerVisible());
  HandlePendingEvents();
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, DevicePickerIsNotShownWithoutAudioThread) {
  // There is no one to ask for devices
  EXPECT_TRUE(Send(interface::keybinding::General::ChangeAudioDevice));
  EXPECT_FALSE(IsDevicePickerVisible());
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, RestoreDeviceFromSettings) {
  using ::testing::_;

  const std::string device{"front:CARD=DAC,DEV=0"};

  // Device from last run is already in use by audio thread, so there is nothing to send to it
  ON_CALL(*file_handler, ParseSettings(_))
      .WillByDefault(::testing::DoAll(
          ::testing::SetArgReferee<0>(model::Settings{.device = device}), ::testing::Return(true)));
  CreateTerminal();

  EXPECT_CALL(*notifier, SetAudioDevice(_)).Times(0);
  RegisterNotifier();
  HandlePendingEvents();

  ::testing::Mock::VerifyAndClearExpectations(notifier.get());

  // And it is the selected one in picker (even when it is not the first device)
  EXPECT_CALL(*notifier, GetAudioDevices())
      .WillOnce(::testing::Return(model::AudioDevices{{.name = "default"}, {.name = device}}));

  Send(interface::keybinding::General::ChangeAudioDevice);
  EXPECT_THAT(Render(), ::testing::HasSubstr("▶ " + device));
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ShowQuestionDialog) {
  bool answered = false;

  terminal->ProcessEvent(interface::CustomEvent::ShowQuestionDialog(model::QuestionData{
      .question = "Do you want to quit?",
      .cb_yes = [&answered] { answered = true; },
  }));

  EXPECT_TRUE(IsQuestionVisible());
  EXPECT_THAT(Render(), ::testing::HasSubstr("Do you want to quit?"));

  EXPECT_TRUE(Send(interface::keybinding::Dialog::Yes));

  EXPECT_TRUE(answered);
  EXPECT_FALSE(IsQuestionVisible());
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ShowPlaylistDialog) {
  terminal->ProcessEvent(interface::CustomEvent::ShowPlaylistManager(model::PlaylistOperation{
      .action = model::PlaylistOperation::Operation::Create,
  }));

  EXPECT_TRUE(IsPlaylistDialogVisible());

  // Keys go to dialog while it is opened
  Send(interface::keybinding::Navigation::Tab);
  EXPECT_EQ(GetFocusedIndex(), kSidebar);

  EXPECT_TRUE(Send(interface::keybinding::Navigation::Escape));
  EXPECT_FALSE(IsPlaylistDialogVisible());
}

/* ********************************************************************************************** */

TEST_F(TerminalTest, ToggleFullscreen) {
  using interface::CustomEvent;

  const int bars = terminal->CalculateNumberBars();

  terminal->ProcessEvent(CustomEvent::ToggleFullscreen());
  EXPECT_TRUE(IsFullscreen());

  // Only spectrum visualizer is rendered, using the whole terminal
  std::string rendered = Render();
  EXPECT_THAT(rendered, ::testing::Not(::testing::HasSubstr("F1:files")));
  EXPECT_THAT(rendered, ::testing::Not(::testing::HasSubstr(" player ")));

  EXPECT_GT(terminal->CalculateNumberBars(), bars);

  // And it fits even in a small terminal
  Resize(kSmallColumns, kSmallLines);
  EXPECT_THAT(Render(), ::testing::Not(::testing::HasSubstr("Terminal too small")));

  // Focus cannot be changed, as other blocks are not visible
  EXPECT_FALSE(Send(interface::keybinding::Navigation::Tab));
  EXPECT_EQ(GetFocusedIndex(), kSidebar);

  // But media player still handles its keys
  RegisterNotifier();
  EXPECT_CALL(*notifier, SetShuffle(true));

  EXPECT_TRUE(Send(interface::keybinding::MediaPlayer::ToggleShuffle));
  HandlePendingEvents();

  // Back to normal
  Resize(kColumns, kLines);
  terminal->ProcessEvent(CustomEvent::ToggleFullscreen());

  EXPECT_FALSE(IsFullscreen());
  EXPECT_THAT(Render(), ::testing::HasSubstr(" player "));
}

}  // namespace
