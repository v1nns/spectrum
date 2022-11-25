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

namespace {

using ::testing::_;
using ::testing::Field;
using ::testing::InSequence;

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

TEST_F(MediaControllerTest, ExecuteAllMethods) {
  // TODO: call entire API
}

}  // namespace
