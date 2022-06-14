/**
 * \file
 * \brief  Mock class for List Directory block
 */

#ifndef INCLUDE_TEST_LIST_DIRECTORY_MOCK_H_
#define INCLUDE_TEST_LIST_DIRECTORY_MOCK_H_

#include <gmock/gmock-actions.h>          // for GMOCK_PP_INTERNAL_IF_0, GMO...
#include <gmock/gmock-function-mocker.h>  // for GMOCK_INTERNAL_DETECT_OVERR...
#include <gmock/gmock-spec-builders.h>    // for FunctionMocker, MockSpec

#include <memory>  // for shared_ptr
#include <string>  // for string

#include "view/block/list_directory.h"  // for ListDirectory

namespace {

//! Implement custom action to show only directory filename instead of the full path
ACTION_P(ReturnPointee, p) { return p->filename().string(); }

//! Mock class to change default behaviour when rendering the inner element corresponding to Title
class ListDirectoryMock final : public interface::ListDirectory {
 public:
  ListDirectoryMock(const std::shared_ptr<interface::EventDispatcher>& d, const std::string& s)
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
#endif  // INCLUDE_TEST_LIST_DIRECTORY_MOCK_H_
