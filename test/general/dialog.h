/**
 * \file
 * \brief  Interface class for UI testing
 */

#ifndef INCLUDE_TEST_GENERAL_DIALOG_H_
#define INCLUDE_TEST_GENERAL_DIALOG_H_

#include <gmock/gmock-matchers.h>

#include <memory>

#include "mock/event_dispatcher_mock.h"
#include "util/logger.h"
#include "view/base/dialog.h"

namespace {

/**
 * @brief Interface class for tests with dialog component from UI
 */
class DialogTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  virtual void SetUp() override = 0;

  void TearDown() override {
    screen.reset();
    dispatcher.reset();
    dialog.reset();
  }

 protected:
  std::unique_ptr<ftxui::Screen> screen;
  std::shared_ptr<EventDispatcherMock> dispatcher;
  std::shared_ptr<interface::Dialog> dialog;
};

}  // namespace
#endif  // INCLUDE_TEST_GENERAL_DIALOG_H_
