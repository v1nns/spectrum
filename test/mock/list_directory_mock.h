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

#include "view/block/sidebar_content/list_directory.h"  // for ListDirectory

namespace {

using ::testing::AtLeast;

//! Implement custom action to show only directory filename instead of the full path
ACTION_P(ReturnPointee, p) { return p->filename().string(); }

//! Mock class to change default behaviour when rendering the inner element corresponding to Title
class ListDirectoryMock final : public interface::ListDirectory {
 public:
  explicit ListDirectoryMock(const model::BlockIdentifier& id,
                             const std::shared_ptr<interface::EventDispatcher>& dispatcher,
                             const FocusCallback& on_focus,
                             const interface::keybinding::Key& keybinding, int max_columns,
                             const std::string& optional_path = "")
      : interface::ListDirectory(id, dispatcher, on_focus, keybinding, max_columns, optional_path) {
    SetupTitleExpectation();
  }

  MOCK_METHOD(std::string, GetTitle, (), (override));

  void SetupTitleExpectation() {
    ON_CALL(*this, GetTitle()).WillByDefault(ReturnPointee(&GetCurrentDir()));

    // Instead of using NiceMock to ignore uninteresting calls from "GetTitle"
    // create this expectation for every Render()
    EXPECT_CALL(*this, GetTitle).Times(AtLeast(1));
  }
};

}  // namespace
#endif  // INCLUDE_TEST_LIST_DIRECTORY_MOCK_H_
