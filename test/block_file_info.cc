#include <gmock/gmock-matchers.h>

#include "general/block.h"
#include "general/utils.h"
#include "mock/event_dispatcher_mock.h"
#include "view/block/file_info.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::StrEq;
using ::testing::VariantWith;

/**
 * @brief Tests with FileInfo class
 */
class FileInfoTest : public ::BlockTest {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(32, 11);

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
│Nothing playing               │
│Press Return to play a song   │
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

  // Use the whole block width (content + border)
  screen = std::make_unique<ftxui::Screen>(38, 11);

  // Process custom event on block
  auto event = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event);

  // Audio output is informed right after song (it does not use the same sample rate when it is
  // not supported by output device)
  Process(interface::CustomEvent::UpdateAudioOutput(model::AudioOutput{
      .device = "front:CARD=DAC,DEV=0",
      .format = model::AudioFormat{.sample_rate = 96000, .sample_format = model::SampleFormat::S32},
  }));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ information ───────────────────────╮
│Lágrimas                            │
│Baco Exu do Blues                   │
│                                    │
│file     song.mp3                   │
│format   44.1 kHz · 32 bits · stereo│
│bitrate  256 kbps                   │
│length   02:03                      │
│output   96 kHz / 32 bits           │
│device   front:CARD=DAC,DEV=0       │
╰────────────────────────────────────╯)";

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

  Process(interface::CustomEvent::UpdateAudioOutput(model::AudioOutput{.device = "default"}));

  // Process custom event on block
  auto event_clear = interface::CustomEvent::ClearSongInfo();
  Process(event_clear);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ information ─────────────────╮
│Nothing playing               │
│Press Return to play a song   │
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

TEST_F(FileInfoTest, TruncateLongValuesWithEllipsis) {
  // Use the whole block width (content + border)
  screen = std::make_unique<ftxui::Screen>(38, 11);

  const model::Song audio{
      .filepath = "/music/Zzqx Unknown Artist - No Such Song Qwerty.mp3",
      .artist = "ARTY",
      .title = "日本語のとても長い曲のタイトルです、本当に",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 128000,
      .bit_depth = 32,
      .duration = 30,
  };

  Process(interface::CustomEvent::UpdateSongInfo(audio));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  // Long filename is cut with an ellipsis
  EXPECT_THAT(rendered, HasSubstr("│file     Zzqx Unknown Artist - No S…│"));

  // Full-width characters use two columns each, so title is cut without breaking any of them
  EXPECT_THAT(rendered, HasSubstr("│日本語のとても長い曲のタイトルです… │"));

  // Short values are not changed
  EXPECT_THAT(rendered, HasSubstr("│ARTY "));
  EXPECT_THAT(rendered, HasSubstr("│format   44.1 kHz · 32 bits · stereo│"));
}

/* ********************************************************************************************** */

TEST_F(FileInfoTest, UpdateAudioOutput) {
  // Use the whole block width (content + border)
  screen = std::make_unique<ftxui::Screen>(38, 11);

  Process(interface::CustomEvent::UpdateSongInfo(model::Song{.filepath = "/music/song.flac"}));
  Process(interface::CustomEvent::UpdateAudioOutput(model::AudioOutput{.device = "default"}));

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("│output   44.1 kHz / 16 bits         │"));
  EXPECT_THAT(rendered, HasSubstr("│device   default                    │"));

  // Without tags, filename is used as title
  EXPECT_THAT(rendered, HasSubstr("│song.flac "));
  EXPECT_THAT(rendered, HasSubstr("│Unknown artist "));

  // Output device may be changed while song is playing, which may also change the format
  Process(interface::CustomEvent::UpdateAudioOutput(model::AudioOutput{
      .device = "iec958:CARD=SomeVeryLongCardName,DEV=0",
      .format =
          model::AudioFormat{.sample_rate = 192000, .sample_format = model::SampleFormat::S32},
  }));

  screen->Clear();
  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("│output   192 kHz / 32 bits          │"));

  // Long device name is cut with an ellipsis
  EXPECT_THAT(rendered, HasSubstr("│device   iec958:CARD=SomeVeryLongCa…│"));
}

/* ********************************************************************************************** */

TEST_F(FileInfoTest, ShowLossySongWithLongDuration) {
  // Use the whole block width (content + border)
  screen = std::make_unique<ftxui::Screen>(38, 11);

  // Lossy formats (e.g. MP3) do not have bit depth, so decoder reports it as zero
  const model::Song audio{
      .filepath = "/music/podcast.mp3",
      .artist = "ARTY",
      .title = "Long episode",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 128000,
      .bit_depth = 0,
      .duration = 3723,
  };

  Process(interface::CustomEvent::UpdateSongInfo(audio));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  // Bit depth is not shown at all
  EXPECT_THAT(rendered, HasSubstr("│format   44.1 kHz · stereo          │"));
  EXPECT_THAT(rendered, HasSubstr("│length   01:02:03                   │"));
}

/* ********************************************************************************************** */

TEST_F(FileInfoTest, MouseOnTitle) {
  using interface::CustomEvent;
  utils::ThemeGuard guard;

  //! Position of block title on screen
  static constexpr int kTitleRow = 0;
  static constexpr int kTitleColumn = 2;

  const auto hovered = utils::MarkerColor(1);

  //! Simulate a mouse event on the given position, and render block
  auto send_mouse = [this](int x, int y, ftxui::Mouse::Button button, ftxui::Mouse::Motion motion) {
    bool handled = block->OnEvent(ftxui::Event::Mouse(
        "", ftxui::Mouse{.button = button, .motion = motion, .x = x, .y = y}));

    screen->Clear();
    ftxui::Render(*screen, block->Render());

    return handled;
  };

  // Title is hovered like the tab selected from other blocks
  interface::Theme theme;
  theme.block.tab.selected = interface::Theme::State{.foreground = hovered, .background = hovered};
  interface::SetTheme(theme);

  ftxui::Render(*screen, block->Render());
  EXPECT_FALSE(utils::HasColor(*screen, hovered));

  EXPECT_FALSE(send_mouse(kTitleColumn, kTitleRow, ftxui::Mouse::None, ftxui::Mouse::Pressed));
  EXPECT_TRUE(utils::HasColor(*screen, hovered));

  EXPECT_FALSE(send_mouse(kTitleColumn, kTitleRow + 1, ftxui::Mouse::None, ftxui::Mouse::Pressed));
  EXPECT_FALSE(utils::HasColor(*screen, hovered));

  // A click anywhere else is not handled
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  EXPECT_FALSE(send_mouse(kTitleColumn, kTitleRow + 1, ftxui::Mouse::Left, ftxui::Mouse::Released));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // And a click on title asks for focus, when another block is the one focused
  std::static_pointer_cast<interface::Block>(block)->SetFocused(false);

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&CustomEvent::id, CustomEvent::Identifier::SetFocused),
                              Field(&CustomEvent::content, VariantWith<model::BlockIdentifier>(
                                                               model::BlockIdentifier::FileInfo)))));

  EXPECT_TRUE(send_mouse(kTitleColumn, kTitleRow, ftxui::Mouse::Left, ftxui::Mouse::Released));
}

}  // namespace
