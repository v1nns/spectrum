#include "ui/block/list_directory.h"

#include <gmock/gmock-actions.h>          // for GMOCK_PP_INTERNAL_IF_0
#include <gmock/gmock-function-mocker.h>  // for GMOCK_INTERNAL_DETECT_...
#include <gmock/gmock-matchers.h>         // for StrEq, EXPECT_THAT
#include <gmock/gmock-nice-strict.h>      // for NiceMock
#include <gmock/gmock-spec-builders.h>    // for FunctionMocker, MockSpec
#include <gtest/gtest-message.h>          // for Message
#include <gtest/gtest-test-part.h>        // for TestPartResult

#include <memory>  // for __shared_ptr_access

#include "ftxui/component/component.hpp"       // for Make
#include "ftxui/component/component_base.hpp"  // for Component, ComponentBase
#include "ftxui/component/event.hpp"           // for Event, Event::ArrowDown
#include "ftxui/dom/node.hpp"                  // for Render
#include "ftxui/screen/screen.hpp"             // for Screen
#include "gtest/gtest_pred_impl.h"             // for SuiteApiResolver, TEST_F
#include "utils.h"                             // for FilterAnsiCommands

namespace {

using ::testing::NiceMock;  //!< Using NiceMock to ignore uninteresting calls from "GetTitle"
using ::testing::StrEq;

//! Implement custom action to show only directory filename instead of the full path
ACTION_P(ReturnPointee, p) { return p->filename().string(); }

//! Mock class to change default behaviour while rendering the inner element Title
class MockListDirectory : public interface::ListDirectory {
 public:
  MockListDirectory(const std::string& s) : interface::ListDirectory(s) { SetupTitleExpectation(); }

  MOCK_METHOD(std::string, GetTitle, (), (override));

  void SetupTitleExpectation() {
    ON_CALL(*this, GetTitle()).WillByDefault(ReturnPointee(&curr_dir_));
  }
};

/* ********************************************************************************************** */

/**
 * @brief Tests with ListDirectory class
 */
class ListDirectoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(32, 15);

    std::string source_dir{std::filesystem::current_path().parent_path()};
    block = ftxui::Make<NiceMock<MockListDirectory>>(source_dir);
  }

  void TearDown() override {
    screen.reset();
    block.reset();
  }

 protected:
  std::unique_ptr<ftxui::Screen> screen;
  ftxui::Component block;
};

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, InitialRender) {
  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ Files ───────────────────────╮
│spectrum                      │
│> ..                          │
│  build                       │
│  .clang-format               │
│  CMakeLists.txt              │
│  .git                        │
│  .gitignore                  │
│  include                     │
│  README.md                   │
│  src                         │
│  test                        │
│  .vscode                     │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NavigateOnMenu) {
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);

  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ Files ───────────────────────╮
│spectrum                      │
│  ..                          │
│  build                       │
│  .clang-format               │
│> CMakeLists.txt              │
│  .git                        │
│  .gitignore                  │
│  include                     │
│  README.md                   │
│  src                         │
│  test                        │
│  .vscode                     │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NavigateToTestDir) {
  block->OnEvent(ftxui::Event::End);
  block->OnEvent(ftxui::Event::ArrowUp);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ Files ───────────────────────╮
│test                          │
│> ..                          │
│  CMakeLists.txt              │
│  list_directory.cc           │
│  utils.h                     │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, EnterOnSearchMode) {
  block->OnEvent(ftxui::Event::Character('/'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ Files ───────────────────────╮
│spectrum                      │
│> ..                          │
│  build                       │
│  .clang-format               │
│  CMakeLists.txt              │
│  .git                        │
│  .gitignore                  │
│  include                     │
│  README.md                   │
│  src                         │
│  test                        │
│  .vscode                     │
│Text to search:               │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, SingleCharacterInSearchMode) {
  std::string typed{"/i"};
  QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ Files ───────────────────────╮
│spectrum                      │
│> build                       │
│  CMakeLists.txt              │
│  .git                        │
│  .gitignore                  │
│  include                     │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│Text to search: i             │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, TextAndNavigateInSearchMode) {
  std::string typed{"/test"};
  QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ Files ───────────────────────╮
│test                          │
│> ..                          │
│  CMakeLists.txt              │
│  list_directory.cc           │
│  utils.h                     │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NonExistentTextInSearchMode) {
  std::string typed{"/inexistentfilename"};
  QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ Files ───────────────────────╮
│spectrum                      │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│Text to search: inexistentfile│
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, EnterAndExitSearchMode) {
  block->OnEvent(ftxui::Event::Character('/'));
  block->OnEvent(ftxui::Event::Escape);

  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ Files ───────────────────────╮
│spectrum                      │
│> ..                          │
│  build                       │
│  .clang-format               │
│  CMakeLists.txt              │
│  .git                        │
│  .gitignore                  │
│  include                     │
│  README.md                   │
│  src                         │
│  test                        │
│  .vscode                     │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace