
#include "mock/list_directory.h"

#include <gmock/gmock-matchers.h>   // for StrEq, EXPECT_THAT
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <filesystem>  // for current_path, path
#include <memory>      // for __shared_ptr_access

#include "ftxui/component/component.hpp"       // for Make
#include "ftxui/component/component_base.hpp"  // for Component, ComponentBase
#include "ftxui/component/event.hpp"           // for Event, Event::ArrowDown
#include "ftxui/dom/node.hpp"                  // for Render
#include "ftxui/screen/screen.hpp"             // for Screen
#include "gtest/gtest_pred_impl.h"             // for SuiteApiResolver, TEST_F
#include "utils.h"                             // for FilterAnsiCommands

namespace {

using ::testing::StrEq;

/**
 * @brief Tests with ListDirectory class
 */
class ListDirectoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(32, 15);

    std::string source_dir{std::filesystem::current_path().parent_path()};
    block = ftxui::Make<MockListDirectory>(nullptr, source_dir);
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
╭ files ───────────────────────╮
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
  block->OnEvent(ftxui::Event::Tab);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::TabReverse);
  block->OnEvent(ftxui::Event::ArrowDown);

  ftxui::Render(*screen, block->Render());

  std::string rendered = FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
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
╭ files ───────────────────────╮
│test                          │
│> ..                          │
│  CMakeLists.txt              │
│  list_directory.cc           │
│  mock                        │
│  player.cc                   │
│  utils.h                     │
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
╭ files ───────────────────────╮
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
│Search:                       │
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
╭ files ───────────────────────╮
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
│Search:i                      │
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
╭ files ───────────────────────╮
│test                          │
│> ..                          │
│  CMakeLists.txt              │
│  list_directory.cc           │
│  mock                        │
│  player.cc                   │
│  utils.h                     │
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
╭ files ───────────────────────╮
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
│Search:inexistentfilename     │
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
╭ files ───────────────────────╮
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