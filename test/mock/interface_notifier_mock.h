/**
 * \file
 * \brief  Mock class for Playback API
 */

#ifndef INCLUDE_TEST_INTERFACE_NOTIFIER_MOCK_H_
#define INCLUDE_TEST_INTERFACE_NOTIFIER_MOCK_H_

#include "view/base/interface_notifier.h"

namespace {

class InterfaceNotifierMock final : public interface::InterfaceNotifier {
 public:
  MOCK_METHOD(void, NotifySongInformation, (const model::Song& info), (override));
  MOCK_METHOD(void, ClearSongInformation, (), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_INTERFACE_NOTIFIER_MOCK_H_