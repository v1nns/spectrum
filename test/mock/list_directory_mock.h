/**
 * \file
 * \brief  Mock class for List Directory block
 */

#ifndef INCLUDE_TEST_LIST_DIRECTORY_MOCK_H_
#define INCLUDE_TEST_LIST_DIRECTORY_MOCK_H_

#include <gmock/gmock-actions.h>
#include <gmock/gmock-function-mocker.h>
#include <gmock/gmock-spec-builders.h>

#include <memory>
#include <string>

#include "view/block/sidebar_content/list_directory.h"

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
                             const interface::keybinding::Key& keybinding,
                             const std::shared_ptr<util::FileHandler>& file_handler,
                             int max_columns, const std::string& optional_path = "")
      : interface::ListDirectory(id, dispatcher, on_focus, keybinding, file_handler, max_columns,
                                 optional_path) {
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
