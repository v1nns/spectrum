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

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, UpdateSongInfo) {
  model::Song audio{
      .filepath = "/another/custom/path/to/song.mp3",
      .artist = "Deko",
      .title = "Phantasy Star Online",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 193,
  };

  // Process custom event on block
  auto event = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event);

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
│     00:00                                          03:13     │
│                                                              │
╰──────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, StartPlaying) {
  model::Song audio{
      .filepath = "/another/custom/path/to/music.mp3",
      .artist = "Mr.Kitty",
      .title = "After Dark",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 259,
  };

  // Process custom event on block to update song info
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Play,
      .position = 103,
  };

  // Process custom event on block to update song state
  auto event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────╮
│                                                              │
│                       ╭──────╮╭──────╮                       │
│                       │ ⣶  ⣶ ││ ⣶⣶⣶⣶ │                       │
│                       │ ⣿  ⣿ ││ ⣿⣿⣿⣿ │                       │
│                       │ ⠿  ⠿ ││ ⠿⠿⠿⠿ │                       │
│                       ╰──────╯╰──────╯      Volume: 100%     │
│                                                              │
│     ████████████████████▋                                    │
│     01:43                                          04:19     │
│                                                              │
╰──────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace
