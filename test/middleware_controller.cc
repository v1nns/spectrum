#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "general/sync_testing.h"
#include "middleware/media_controller.h"
#include "mock/analyzer_mock.h"
#include "mock/audio_control_mock.h"
#include "mock/event_dispatcher_mock.h"
#include "model/application_error.h"
#include "view/base/listener.h"
#include "view/base/notifier.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::Return;
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
  void SetUp() override { Init(); }

  void TearDown() override { controller.reset(); }

  void Init(bool asynchronous = false) {
    // Create mocks
    dispatcher = std::make_shared<EventDispatcherMock>();
    audio_ctl = std::make_shared<AudioControlMock>();
    AnalyzerMock* an_mock = new AnalyzerMock();

    // Setup init expectations
    InSequence seq;

    EXPECT_CALL(*dispatcher, CalculateNumberBars()).WillOnce(Return(kNumberBars));
    EXPECT_CALL(*an_mock, Init(Eq(kNumberBars)));
    EXPECT_CALL(*audio_ctl, GetAudioVolume());
    EXPECT_CALL(*dispatcher, ProcessEvent(Field(&interface::CustomEvent::id,
                                                interface::CustomEvent::Identifier::UpdateVolume)));
    EXPECT_CALL(*dispatcher,
                ProcessEvent(Field(&interface::CustomEvent::id,
                                   interface::CustomEvent::Identifier::DrawAudioSpectrum)));

    // Create Controller
    controller = middleware::MediaController::Create(dispatcher, audio_ctl, an_mock, asynchronous);
  }

  //! Getter for Interface Listener
  // P.S.: As controller derives from both Listener and Notifier, must use static_cast for upcasting
  auto GetListener() -> interface::Listener* {
    return static_cast<interface::Listener*>(controller.get());
  }

  //! Getter for Interface Notifier
  auto GetNotifier() -> interface::Notifier* {
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

TEST_F(MediaControllerTest, ExecuteAllMethodsFromListener) {
  auto listener = GetListener();
  auto audio_ctl = GetAudioControl();
  auto analyzer = GetAnalyzer();

  InSequence seq;

  std::filesystem::path music{"/stairway/to/heaven.flac"};
  EXPECT_CALL(*audio_ctl, Play(Eq(music)));
  listener->NotifyFileSelection(music);

  EXPECT_CALL(*audio_ctl, PauseOrResume());
  listener->PauseOrResume();

  EXPECT_CALL(*audio_ctl, Stop());
  listener->Stop();

  EXPECT_CALL(*audio_ctl, Stop());
  listener->ClearCurrentSong();

  model::Volume volume{0.7};
  EXPECT_CALL(*audio_ctl, SetAudioVolume(Eq(volume)));
  listener->SetVolume(volume);

  int number_bars = 16;
  EXPECT_CALL(*analyzer, Init(Eq(number_bars)));
  listener->ResizeAnalysisOutput(number_bars);

  int skip_seconds = 25;
  EXPECT_CALL(*audio_ctl, SeekForwardPosition(Eq(skip_seconds)));
  listener->SeekForwardPosition(skip_seconds);

  EXPECT_CALL(*audio_ctl, SeekBackwardPosition(Eq(skip_seconds)));
  listener->SeekBackwardPosition(skip_seconds);
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, ExecuteAllMethodsFromNotifier) {
  auto notifier = GetNotifier();
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

  // TODO: what should be done on this one?
  //   notifier->SendAudioRaw();

  error::Code error = error::kUnknownError;
  EXPECT_CALL(*dispatcher, SetApplicationError(Eq(error)));
  notifier->NotifyError(error);
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
    auto notifier = GetNotifier();

    // Send random data to the thread to analyze it
    syncer.WaitForStep(1);
    std::vector<int> buffer(sample_size, 1);
    notifier->SendAudioRaw(buffer.data(), buffer.size());

    // Wait for Analysis to finish before exiting from controller
    syncer.WaitForStep(2);
    controller->Exit();
  };

  testing::RunAsyncTest({analysis, client});
}

/* ********************************************************************************************** */

TEST_F(MediaControllerTest, AnalysisAndClearAnimation) {
  int sample_size = 16;
  model::Song::CurrentInformation info{.state = model::Song::MediaState::Pause, .position = 12};

  auto analysis = [&](TestSyncer& syncer) {
    auto analyzer = GetAnalyzer();
    auto dispatcher = GetEventDispatcher();

    EXPECT_CALL(*analyzer, GetBufferSize()).WillRepeatedly(Return(sample_size));
    EXPECT_CALL(*analyzer, GetOutputSize()).WillRepeatedly(Return(kNumberBars));

    {
      // Create expectation to analyze data and send its result back to UI
      EXPECT_CALL(*analyzer, Execute(_, Eq(sample_size), _))
          .WillOnce(Invoke([&](double* input, int, double* output) {
            std::copy(input, input + kNumberBars, reinterpret_cast<double*>(output));
            syncer.NotifyStep(2);
            return error::kSuccess;
          }));

      EXPECT_CALL(
          *dispatcher,
          SendEvent(
              AllOf(Field(&interface::CustomEvent::id,
                          interface::CustomEvent::Identifier::DrawAudioSpectrum),
                    Field(&interface::CustomEvent::content,
                          VariantWith<std::vector<double>>(std::vector<double>(kNumberBars, 1))))));
    }

    {
      // Create expectation to execute Clear Animation and send it to UI
      EXPECT_CALL(*dispatcher,
                  SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                        interface::CustomEvent::Identifier::UpdateSongState),
                                  Field(&interface::CustomEvent::content,
                                        VariantWith<model::Song::CurrentInformation>(info)))));

      // As we can get a lot of DrawAudioSpectrum events, use the NotEqual(vector<1>) to match the
      // result values from ClearAnimation (and differentiate from analyze and last update values)
      EXPECT_CALL(
          *dispatcher,
          SendEvent(AllOf(
              Field(&interface::CustomEvent::id,
                    interface::CustomEvent::Identifier::DrawAudioSpectrum),
              Field(&interface::CustomEvent::content,
                    VariantWith<std::vector<double>>(Ne(std::vector<double>(kNumberBars, 1)))))))
          .Times(10);

      // Last update from thread with zeroed values for UI
      std::vector<double> last_update(kNumberBars, 0.001);
      EXPECT_CALL(*dispatcher,
                  SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                        interface::CustomEvent::Identifier::DrawAudioSpectrum),
                                  Field(&interface::CustomEvent::content,
                                        VariantWith<std::vector<double>>(last_update)))))
          .WillOnce(Invoke([&]() { syncer.NotifyStep(3); }));
    }

    // Notify that expectations are set, and run audio loop
    syncer.NotifyStep(1);
    RunAnalysisLoop();
  };

  auto client = [&](TestSyncer& syncer) {
    auto notifier = GetNotifier();

    // In order to run ClearAnimation, must send some raw data first (to fill internal buffer)
    syncer.WaitForStep(1);
    std::vector<int> buffer(sample_size, 1);
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

}  // namespace
