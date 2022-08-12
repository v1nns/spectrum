/**
 * \file
 * \brief  Mock class for Playback API
 */

#ifndef INCLUDE_TEST_INTERFACE_NOTIFIER_MOCK_H_
#define INCLUDE_TEST_INTERFACE_NOTIFIER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "view/base/notifier.h"

namespace {

class InterfaceNotifierMock final : public interface::Notifier {
 public:
  MOCK_METHOD(void, ClearSongInformation, (), (override));
  MOCK_METHOD(void, NotifySongInformation, (const model::Song& info), (override));
  MOCK_METHOD(void, NotifySongState, (const model::Song::CurrentInformation& new_state), (override));
  MOCK_METHOD(void, SendAudioRaw, (int* buffer, int buffer_size), (override));
  MOCK_METHOD(void, NotifyError, (error::Code code), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_INTERFACE_NOTIFIER_MOCK_H_
