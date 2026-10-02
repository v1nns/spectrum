#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <memory>
#include <vector>

#include "audio/base/notifier.h"
#include "general/sync_testing.h"
#include "middleware/media_controller.h"
#include "mock/analyzer_mock.h"
#include "mock/audio_control_mock.h"
#include "mock/event_dispatcher_mock.h"
#include "model/application_error.h"
#include "model/playlist.h"
#include "model/song.h"
#include "util/logger.h"
#include "view/base/notifier.h"

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

      EXPECT_CALL(*dispatcher,
                  SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                        interface::CustomEvent::Identifier::DrawAudioSpectrum),
                                  Field(&interface::CustomEvent::content,
                                        VariantWith<std::vector<double>>(ElementsAreArray(bars))))));
    }

    // After animation, ask UI to resume song (without running animation again)
    EXPECT_CALL(*dispatcher, SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                                   interface::CustomEvent::Identifier::ResumeSong),
                                             Field(&interface::CustomEvent::content,
                                                   VariantWith<bool>(false)))))
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

}  // namespace
