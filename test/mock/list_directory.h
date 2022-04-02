/**
 * \file
 * \brief  Class representing a single UI block
 */

#ifndef INCLUDE_TEST_MOCK_LIST_DIRECTORY_H_
#define INCLUDE_TEST_MOCK_LIST_DIRECTORY_H_

#include <gmock/gmock-actions.h>          // for GMOCK_PP_INTERNAL_IF_0, GMO...
#include <gmock/gmock-function-mocker.h>  // for GMOCK_INTERNAL_DETECT_OVERR...
#include <gmock/gmock-spec-builders.h>    // for FunctionMocker, MockSpec

#include <memory>  // for shared_ptr
#include <string>  // for string

#include "ui/block/list_directory.h"  // for ListDirectory

namespace {
using interface::Dispatcher;

//! Implement custom action to show only directory filename instead of the full path
ACTION_P(ReturnPointee, p) { return p->filename().string(); }

//! Mock class to change default behaviour when rendering the inner element corresponding to Title
class MockListDirectory : public interface::ListDirectory {
 public:
  MockListDirectory(const std::shared_ptr<interface::Dispatcher>& d, const std::string& s)
      : interface::ListDirectory(d, s) {
    SetupTitleExpectation();
  }

  MOCK_METHOD(std::string, GetTitle, (), (override));

  void SetupTitleExpectation() {
    ON_CALL(*this, GetTitle()).WillByDefault(ReturnPointee(&curr_dir_));

    // Instead of using NiceMock to ignore uninteresting calls from "GetTitle"
    // create this expectation for every Render()
    EXPECT_CALL(*this, GetTitle).Times(1);
  }
};

}  // namespace
#endif  // INCLUDE_TEST_MOCK_LIST_DIRECTORY_H_
