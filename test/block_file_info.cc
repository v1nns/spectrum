#include <gmock/gmock-matchers.h>

#include "general/block.h"
#include "general/utils.h"
#include "mock/event_dispatcher_mock.h"
#include "view/block/file_info.h"

namespace {

using ::testing::HasSubstr;
using ::testing::StrEq;

/**
 * @brief Tests with FileInfo class
 */
class FileInfoTest : public ::BlockTest {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(32, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create FileInfo block
    block = ftxui::Make<interface::FileInfo>(dispatcher);

    // Set this block as focused
    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);
  }
};

/* ********************************************************************************************** */

TEST_F(FileInfoTest, InitialRender) {
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ information ─────────────────╮
│Filename               <Empty>│
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
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(FileInfoTest, UpdateSongInfo) {
  model::Song audio{
      .filepath = "/some/custom/path/to/song.mp3",
      .artist = "Baco Exu do Blues",
      .title = "Lágrimas",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 123,
  };

  // Process custom event on block
  auto event = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ information ─────────────────╮
│Filename              song.mp3│
│Artist       Baco Exu do Blues│
│Title                 Lágrimas│
│Channels                     2│
│Sample rate           44.1 kHz│
│Bit rate              256 kbps│
│Bits per sample        32 bits│
│Duration               123 sec│
│                              │
│                              │
│                              │
│                              │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(FileInfoTest, UpdateAndClearSongInfo) {
  model::Song audio{
      .filepath = "/some/custom/path/to/another/song.mp3",
      .artist = "ARTY",
      .title = "Poison For Lovers",
      .num_channels = 2,
      .sample_rate = 96000,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 123,
  };

  // Process custom event on block
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  // Process custom event on block
  auto event_clear = interface::CustomEvent::ClearSongInfo();
  Process(event_clear);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ information ─────────────────╮
│Filename               <Empty>│
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
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(FileInfoTest, TruncateLongValuesWithEllipsis) {
  // Use the whole block width (content + border)
  screen = std::make_unique<ftxui::Screen>(38, 15);

  const model::Song audio{
      .filepath = "/music/Zzqx Unknown Artist - No Such Song Qwerty.mp3",
      .artist = "ARTY",
      .title = "日本語のとても長い曲のタイトルです",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 128000,
      .bit_depth = 32,
      .duration = 30,
  };

  Process(interface::CustomEvent::UpdateSongInfo(audio));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  // Long filename is cut with an ellipsis, keeping a gap after field name
  EXPECT_THAT(rendered, HasSubstr("│Filename Zzqx Unknown Artist - No S…│"));

  // Full-width characters use two columns each, so title is cut without breaking any of them
  EXPECT_THAT(rendered, HasSubstr("│Title  日本語のとても長い曲のタイト…│"));

  // Short values are not changed
  EXPECT_THAT(rendered, HasSubstr("ARTY│"));
}

}  // namespace
