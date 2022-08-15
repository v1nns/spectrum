#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT

#include "mock/event_dispatcher_mock.h"
#include "utils.h"  // for FilterAnsiCommands
#include "view/block/file_info.h"

namespace {

using ::testing::StrEq;

/**
 * @brief Tests with FileInfo class
 */
class FileInfoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(32, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create FileInfo block
    block = ftxui::Make<interface::FileInfo>(dispatcher);
  }

  void TearDown() override {
    screen.reset();
    dispatcher.reset();
    block.reset();
  }

 protected:
  std::unique_ptr<ftxui::Screen> screen;
  std::shared_ptr<EventDispatcherMock> dispatcher;
  ftxui::Component block;
};

/* ********************************************************************************************** */

TEST_F(FileInfoTest, InitialRender) {
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ information ─────────────────╮
│Artist                 <Empty>│
│Title                  <Empty>│
│Channels               <Empty>│
│Sample rate            <Empty>│
│Bit rate               <Empty>│
│Bits per sample        <Empty>│
│Duration               <Empty>│
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace
