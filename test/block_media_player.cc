#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT

#include "general/block.h"
#include "general/utils.h"  // for FilterAnsiCommands
#include "mock/event_dispatcher_mock.h"
#include "view/block/media_player.h"

namespace {

using ::testing::StrEq;

/**
 * @brief Tests with FileInfo class
 */
class MediaPlayerTest : public ::BlockTest {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(64, 12);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create MediaPlayer block
    block = ftxui::Make<interface::MediaPlayer>(dispatcher);
  }
};

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, InitialRender) {
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────╮
│                                                              │
│                       ╭──────╮╭──────╮                       │
│                       │  ⣦⡀  ││ ⣶⣶⣶⣶ │                       │
│                       │  ⣿⣿⠆ ││ ⣿⣿⣿⣿ │                       │
│                       │  ⠟⠁  ││ ⠿⠿⠿⠿ │                       │
│                       ╰──────╯╰──────╯      Volume: 100%     │
│                                                              │
│                                                              │
│     --:--                                          --:--     │
│                                                              │
╰──────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace
