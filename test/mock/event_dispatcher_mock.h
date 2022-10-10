/**
 * \file
 * \brief  Mock class for UI Event Dispatcher API
 */

#ifndef INCLUDE_TEST_MOCK_EVENT_DISPATCHER_MOCK_H_
#define INCLUDE_TEST_MOCK_EVENT_DISPATCHER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "view/base/event_dispatcher.h"

namespace {

class EventDispatcherMock final : public interface::EventDispatcher {
 public:
  MOCK_METHOD(void, SendEvent, (const interface::CustomEvent& event), (override));
  MOCK_METHOD(void, ProcessEvent, (const interface::CustomEvent& event), (override));
  MOCK_METHOD(void, SetApplicationError, (error::Code id), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_EVENT_DISPATCHER_MOCK_H_
