#include "ui/block/list_directory.h"

#include <memory>
#include <regex>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/screen/screen.hpp"
#include "gtest/gtest.h"

namespace {

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
│  bin                         │
│  build                       │
│  .clang-format               │
│  CMakeLists.txt              │
│  compile_commands.json       │
│  .git                        │
│  .gitignore                  │
│  include                     │
│  lib                         │
│  README.md                   │
│  src                         │
╰──────────────────────────────╯)";

  EXPECT_EQ(rendered, expected);
}

}  // namespace