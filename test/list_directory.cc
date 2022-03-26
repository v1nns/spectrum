#include "ui/block/list_directory.h"

#include <memory>
#include <regex>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/screen/screen.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::StrEq;

//! Filter any ANSI escape code from string
std::string FilterAnsiCommands(const std::string& screen) {
  std::stringstream result;
  const std::regex ansi_command("(\e\\[(\\d+;)*(\\d+)?[ABCDHJKfmsu])|(\\r)");

  std::regex_replace(std::ostream_iterator<char>(result), screen.begin(), screen.end(),
                     ansi_command, "");

  // For aesthetics, add a newline in the beginning
  return result.str().insert(0, 1, '\n');
}

/* ********************************************************************************************** */

void QueueCharacterEvents(ftxui::ComponentBase& block, const std::string& typed) {
  std::for_each(typed.begin(), typed.end(),
                [&block](char const& c) { block.OnEvent(ftxui::Event::Character(c)); });
}

/* ********************************************************************************************** */

/**
 * @brief Tests with ListDirectory class
 */
class ListDirectoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(32, 15);

    std::string source_dir{std::filesystem::current_path().parent_path()};
    block = ftxui::Make<interface::ListDirectory>(source_dir);
  }

  void TearDown() override { block.reset(); }

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
│/home/vinicius/projects/spectr│
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
│/home/vinicius/projects/spectr│
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
│/home/vinicius/projects/spectr│
│> ..                          │
│  CMakeLists.txt              │
│  list_directory.cc           │
│                              │
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
│/home/vinicius/projects/spectr│
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
│/home/vinicius/projects/spectr│
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
│/home/vinicius/projects/spectr│
│> ..                          │
│  CMakeLists.txt              │
│  list_directory.cc           │
│                              │
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
│/home/vinicius/projects/spectr│
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
│/home/vinicius/projects/spectr│
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