#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT
#include <gmock/gmock.h>
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <chrono>
#include <memory>
#include <thread>

#include "general/sync_testing.h"
#include "middleware/media_controller.h"
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

 protected:
  void SetUp() override { Init(); }

  void TearDown() override { controller.reset(); }

  void Init(bool asynchronous = false) {
    dispatcher = std::make_shared<EventDispatcherMock>();
    audio_ctl = std::make_shared<AudioControlMock>();

    // Setup init expectations
    InSequence seq;

    EXPECT_CALL(*dispatcher, CalculateNumberBars());
    EXPECT_CALL(*audio_ctl, GetAudioVolume());
    EXPECT_CALL(*dispatcher, ProcessEvent(Field(&interface::CustomEvent::id,
                                                interface::CustomEvent::Identifier::UpdateVolume)));
    EXPECT_CALL(*dispatcher,
                ProcessEvent(Field(&interface::CustomEvent::id,
                                   interface::CustomEvent::Identifier::DrawAudioSpectrum)));

    // Create Controller
    controller = middleware::MediaController::Create(dispatcher, audio_ctl, asynchronous);
  }

  //! Getter for Event Dispatcher (necessary as inner variable is an unique_ptr)
  auto GetEventDispatcher() -> EventDispatcherMock* {
    auto dummy = controller->dispatcher_.lock();
    return reinterpret_cast<EventDispatcherMock*>(dummy.get());
  }

  //! Getter for Audio Player (necessary as inner variable is an unique_ptr)
  auto GetAudioControl() -> AudioControlMock* {
    auto dummy = controller->player_ctl_.lock();
    return reinterpret_cast<AudioControlMock*>(dummy.get());
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

  //! Run analysis loop (same one executed as a thread in the real-life)
  void RunAnalysisLoop() { controller->AnalysisHandler(); }

 protected:
  EventDispatcher dispatcher;  //!< Base class for terminal (graphical interface)
  AudioControl audio_ctl;      //!< Base class for audio player
  MediaController controller;  //!< Middleware between audio player and graphical interface
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

  InSequence seq;

  std::filesystem::path music{"/stairway/to/heaven.mp3"};
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
  // TODO: Fix some expectation here
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

}  // namespace
