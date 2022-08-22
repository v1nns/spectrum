/**
 * \file
 * \brief  Interface class for UI testing
 */

#ifndef INCLUDE_TEST_GENERAL_BLOCK_H_
#define INCLUDE_TEST_GENERAL_BLOCK_H_

#include <gmock/gmock-matchers.h>

#include "mock/event_dispatcher_mock.h"

namespace {

/**
 * @brief Interface class for tests with block component from UI
 */
class BlockTest : public ::testing::Test {
 protected:
  virtual void SetUp() override = 0;

  void TearDown() override {
    screen.reset();
    dispatcher.reset();
    block.reset();
  }

  void Process(interface::CustomEvent event) {
    auto component = std::static_pointer_cast<interface::Block>(block);
    component->OnCustomEvent(event);
  }

 protected:
  std::unique_ptr<ftxui::Screen> screen;
  std::shared_ptr<EventDispatcherMock> dispatcher;
  ftxui::Component block;
};

}  // namespace
#endif  // INCLUDE_TEST_GENERAL_BLOCK_H_
