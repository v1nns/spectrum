
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
#include "mock/list_directory_mock.h"
#include "utils.h"  // for FilterAnsiCommands

namespace {

using ::testing::StrEq;

/**
 * @brief Tests with ListDirectory class
 */
class ListDirectoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(32, 15);

    // Use test directory as base dir
    std::string source_dir{std::filesystem::current_path().parent_path().string() + "/test"};
    block = ftxui::Make<ListDirectoryMock>(nullptr, source_dir);
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

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│> ..                          │
│  audio_player.cc             │
│  block_list_directory.cc     │
│  CMakeLists.txt              │
│  mock                        │
│  sync_testing.h              │
│  utils.h                     │
│                              │
│                              │
│                              │
│                              │
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

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│  ..                          │
│  audio_player.cc             │
│  block_list_directory.cc     │
│> CMakeLists.txt              │
│  mock                        │
│  sync_testing.h              │
│  utils.h                     │
│                              │
│                              │
│                              │
│                              │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NavigateToMockDir) {
  block->OnEvent(ftxui::Event::End);
  block->OnEvent(ftxui::Event::ArrowUp);
  block->OnEvent(ftxui::Event::ArrowUp);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│mock                          │
│> ..                          │
│  decoder_mock.h              │
│  interface_notifier_mock.h   │
│  list_directory_mock.h       │
│  playback_mock.h             │
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

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│> ..                          │
│  audio_player.cc             │
│  block_list_directory.cc     │
│  CMakeLists.txt              │
│  mock                        │
│  sync_testing.h              │
│  utils.h                     │
│                              │
│                              │
│                              │
│                              │
│Search:                       │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, SingleCharacterInSearchMode) {
  std::string typed{"/e"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│> audio_player.cc             │
│  block_list_directory.cc     │
│  CMakeLists.txt              │
│  sync_testing.h              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│Search:e                      │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, TextAndNavigateInSearchMode) {
  std::string typed{"/mock"};
  utils::QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│mock                          │
│> ..                          │
│  decoder_mock.h              │
│  interface_notifier_mock.h   │
│  list_directory_mock.h       │
│  playback_mock.h             │
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
  utils::QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
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

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│> ..                          │
│  audio_player.cc             │
│  block_list_directory.cc     │
│  CMakeLists.txt              │
│  mock                        │
│  sync_testing.h              │
│  utils.h                     │
│                              │
│                              │
│                              │
│                              │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace
