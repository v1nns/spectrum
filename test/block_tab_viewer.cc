#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT

#include <memory>

#include "audio/lyric/search_config.h"
#include "general/block.h"
#include "general/utils.h"  // for FilterAnsiCommands
#include "mock/event_dispatcher_mock.h"
#include "mock/lyric_finder_mock.h"
#include "util/logger.h"
#include "view/block/tab_item/song_lyric.h"
#include "view/block/tab_item/spectrum_visualizer.h"
#include "view/block/tab_viewer.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::VariantWith;

/**
 * @brief Tests with TabViewer class
 */
class TabViewerTest : public ::BlockTest {
 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(95, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create TabViewer block
    block = ftxui::Make<interface::TabViewer>(dispatcher);

    // Set this block as focused
    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);

    // As we dot want to use dependency injection for Tabview::SongLyrics, we will override
    // LyricFinder manually...  First of all, get tab viewer
    auto tab_viewer = static_cast<interface::TabViewer*>(block.get());

    // Then get song lyric tab item
    auto song_lyric = static_cast<interface::SongLyric*>(
        tab_viewer->views_[interface::TabViewer::View::Lyric].item.get());

    // And finally, override lyric finder to use a mock
    song_lyric->finder_ = std::make_unique<LyricFinderMock>();
  }

  //! Getter for LyricFinder (necessary as inner variable is an unique_ptr)
  auto GetFinder() -> LyricFinderMock* {
    // Get tab viewer
    auto tab_viewer = static_cast<interface::TabViewer*>(block.get());

    // Get song lyric tab item
    auto song_lyric = static_cast<interface::SongLyric*>(
        tab_viewer->views_[interface::TabViewer::View::Lyric].item.get());

    // Return lyric finder mock
    return static_cast<LyricFinderMock*>(song_lyric->finder_.get());
  }

  static constexpr int kNumberBars = 22;  //!< Number of bars for visualizer tab view
};

/* ********************************************************************************************** */

TEST_F(TabViewerTest, InitialRender) {
  auto event_bars =
      interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(kNumberBars, 0.001));
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│   ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁   │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, AnimationHorizontalMirror) {
  std::vector<double> values{0.99, 0.90, 0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.24, 0.15, 0.06,
                             0.99, 0.90, 0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.24, 0.15, 0.06};

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                           ▇▇▇ ▇▇▇                                           │
│                                       ▆▆▆ ███ ███ ▆▆▆                                       │
│                                   ▅▅▅ ███ ███ ███ ███ ▅▅▅                                   │
│                               ▃▃▃ ███ ███ ███ ███ ███ ███ ▃▃▃                               │
│                               ███ ███ ███ ███ ███ ███ ███ ███                               │
│                           ███ ███ ███ ███ ███ ███ ███ ███ ███ ███                           │
│                       ▇▇▇ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▇▇▇                       │
│                   ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▃▃▃                   │
│               ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▃▃▃               │
│           ▁▁▁ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▁▁▁           │
│           ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███           │
│       ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███       │
│   ▇▇▇ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▇▇▇   │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, AnimationVerticalMirror) {
  std::vector<double> values{0.1, 0.2, 0.3,  0.4, 0.5,  0.4, 0.3,  0.2, 0.1,  0.2, 0.3,
                             0.4, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95,

                             0.1, 0.2, 0.3,  0.4, 0.5,  0.4, 0.3,  0.2, 0.1,  0.2, 0.3,
                             0.4, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95};

  // Expect block to send an event to terminal when 'a' is pressed
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(
                  Field(&interface::CustomEvent::id,
                        interface::CustomEvent::Identifier::ChangeBarAnimation),
                  Field(&interface::CustomEvent::content,
                        VariantWith<model::BarAnimation>(model::BarAnimation::VerticalMirror)))));

  block->OnEvent(ftxui::Event::Character('a'));

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  // Maybe filtering ansi commands is messing up with this animation =(
  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                               ▁▁▁ ▄▄▄ ▆▆▆   │
│                                                                   ▂▂▂ ▄▄▄ ▇▇▇ ███ ███ ███   │
│                                                       ▃▃▃ ▅▅▅ ███ ███ ███ ███ ███ ███ ███   │
│               ▄▄▄ ███ ▄▄▄                     ▄▄▄ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███   │
│       ▂▂▂ ▇▇▇ ███ ███ ███ ▇▇▇ ▂▂▂     ▂▂▂ ▇▇▇ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███   │
│   ▅▅▅ ███ ███ ███ ███ ███ ███ ███ ▅▅▅ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███   │
│   ▃▃▃                             ▃▃▃                                                       │
│   ███ ▅▅▅                     ▅▅▅ ███ ▅▅▅                                                   │
│   ███ ███ ███ ▂▂▂     ▂▂▂ ███ ███ ███ ███ ███ ▂▂▂                                           │
│   ███ ███ ███ ███ ▄▄▄ ███ ███ ███ ███ ███ ███ ███ ▄▄▄ ▂▂▂                                   │
│   ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▇▇▇ ▄▄▄ ▁▁▁                       │
│   ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▆▆▆ ▄▄▄ ▁▁▁           │
│   ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▆▆▆ ▃▃▃   │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, AnimationMono) {
  std::vector<double> values{0.1, 0.2,  0.3, 0.4,  0.5, 0.6,  0.5, 0.4, 0.3, 0.2, 0.1,
                             0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.6, 0.7, 0.8, 0.9,

                             0.1, 0.2,  0.3, 0.4,  0.5, 0.6,  0.5, 0.4, 0.3, 0.2, 0.1,
                             0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.6, 0.7, 0.8, 0.9};

  // Expect block to send an event to terminal for each time that 'a' is pressed
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(
                  Field(&interface::CustomEvent::id,
                        interface::CustomEvent::Identifier::ChangeBarAnimation),
                  Field(&interface::CustomEvent::content,
                        VariantWith<model::BarAnimation>(model::BarAnimation::VerticalMirror)))));

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ChangeBarAnimation),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::BarAnimation>(model::BarAnimation::Mono)))));

  block->OnEvent(ftxui::Event::Character('a'));
  block->OnEvent(ftxui::Event::Character('a'));

  // Send event to fill internal data to use it later for rendering animation
  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                       ▆▆▆   │
│                                                                                   ▄▄▄ ███   │
│                                                                               ▁▁▁ ███ ███   │
│                                                                               ███ ███ ███   │
│                       ▇▇▇                                                 ▇▇▇ ███ ███ ███   │
│                   ▄▄▄ ███ ▄▄▄                                         ▄▄▄ ███ ███ ███ ███   │
│               ▂▂▂ ███ ███ ███ ▂▂▂                             ▂▂▂ ▇▇▇ ███ ███ ███ ███ ███   │
│               ███ ███ ███ ███ ███                         ▅▅▅ ███ ███ ███ ███ ███ ███ ███   │
│           ███ ███ ███ ███ ███ ███ ███             ▂▂▂ ███ ███ ███ ███ ███ ███ ███ ███ ███   │
│       ▅▅▅ ███ ███ ███ ███ ███ ███ ███ ▅▅▅     ▅▅▅ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███   │
│   ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███   │
│   ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███   │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, IncreaseAndDecreaseBarWidth) {
  std::vector<double> values{0.99, 0.90, 0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.24, 0.15, 0.06,
                             0.99, 0.90, 0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.24, 0.15, 0.06};

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                           ▇▇▇ ▇▇▇                                           │
│                                       ▆▆▆ ███ ███ ▆▆▆                                       │
│                                   ▅▅▅ ███ ███ ███ ███ ▅▅▅                                   │
│                               ▃▃▃ ███ ███ ███ ███ ███ ███ ▃▃▃                               │
│                               ███ ███ ███ ███ ███ ███ ███ ███                               │
│                           ███ ███ ███ ███ ███ ███ ███ ███ ███ ███                           │
│                       ▇▇▇ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▇▇▇                       │
│                   ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▃▃▃                   │
│               ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▃▃▃               │
│           ▁▁▁ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▁▁▁           │
│           ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███           │
│       ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███       │
│   ▇▇▇ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▇▇▇   │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectations (it will call only once because of internal min-max values)
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::UpdateBarWidth)))
      .Times(1);

  // Increase bar width
  block->OnEvent(ftxui::Event::Character('>'));
  block->OnEvent(ftxui::Event::Character('>'));

  values = std::vector<double>{0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.24, 0.15, 0.06,
                               0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.24, 0.15, 0.06};

  event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  // Clear screen and render again
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                          ▅▅▅▅ ▅▅▅▅                                          │
│                                     ▃▃▃▃ ████ ████ ▃▃▃▃                                     │
│                                     ████ ████ ████ ████                                     │
│                                ████ ████ ████ ████ ████ ████                                │
│                           ▇▇▇▇ ████ ████ ████ ████ ████ ████ ▇▇▇▇                           │
│                      ▃▃▃▃ ████ ████ ████ ████ ████ ████ ████ ████ ▃▃▃▃                      │
│                 ▃▃▃▃ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ▃▃▃▃                 │
│            ▁▁▁▁ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ▁▁▁▁            │
│            ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████            │
│       ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████       │
│  ▇▇▇▇ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ▇▇▇▇  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectations (it will call two times because of internal min-max values)
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::UpdateBarWidth)))
      .Times(2);

  // Decrease bar width
  block->OnEvent(ftxui::Event::Character('<'));
  block->OnEvent(ftxui::Event::Character('<'));
  block->OnEvent(ftxui::Event::Character('<'));

  values = std::vector<double>{
      0.99, 0.90, 0.80, 0.70, 0.60, 0.48, 0.40, 0.35, 0.30, 0.24, 0.20, 0.15, 0.10, 0.06, 0.02,
      0.99, 0.90, 0.80, 0.70, 0.60, 0.48, 0.40, 0.35, 0.30, 0.24, 0.20, 0.15, 0.10, 0.06, 0.02,
  };

  event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  // Clear screen and render again
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                            ▇▇ ▇▇                                            │
│                                         ▆▆ ██ ██ ▆▆                                         │
│                                      ▄▄ ██ ██ ██ ██ ▄▄                                      │
│                                   ▁▁ ██ ██ ██ ██ ██ ██ ▁▁                                   │
│                                   ██ ██ ██ ██ ██ ██ ██ ██                                   │
│                                ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ▇▇                                │
│                             ▂▂ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▂▂                             │
│                          ▂▂ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▂▂                          │
│                       ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▅▅                       │
│                 ▁▁ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▁▁                 │
│              ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▅▅              │
│        ▃▃ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▃▃        │
│  ▃▃ ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▇▇ ▃▃  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, RenderEqualizer) {
  block->OnEvent(ftxui::Event::Character('2'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│                                                                                             │
│╭─────────────╮                                                                              │
││→ Custom     │                                                                              │
│╰─────────────╯   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
│                  ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
│                                                                                             │
│                  0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB   0 dB│
│                                                                                             │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, ModifyEqualizerAndApply) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Change 64Hz frequency (using keybindings for frequency navigation)
  std::string typed{"lllkkkkk"};
  utils::QueueCharacterEvents(*block, typed);

  // Change 250Hz frequency
  typed = "lljj";
  utils::QueueCharacterEvents(*block, typed);

  // Change 1kHz frequency
  block->OnEvent(ftxui::Event::ArrowRight);
  block->OnEvent(ftxui::Event::ArrowRight);
  block->OnEvent(ftxui::Event::Character('j'));
  block->OnEvent(ftxui::Event::Character('j'));
  block->OnEvent(ftxui::Event::Character('j'));

  // Change 4kHz frequency
  typed = "llkkkkkkk";
  utils::QueueCharacterEvents(*block, typed);

  // Setup expectation for event with new audio filters applied
  using model::AudioFilter;
  using model::EqualizerPreset;
  EqualizerPreset audio_filters{
      AudioFilter{.frequency = 32},   AudioFilter{.frequency = 64, .gain = 5},
      AudioFilter{.frequency = 125},  AudioFilter{.frequency = 250, .gain = -2},
      AudioFilter{.frequency = 500},  AudioFilter{.frequency = 1000, .gain = -3},
      AudioFilter{.frequency = 2000}, AudioFilter{.frequency = 4000, .gain = 7},
      AudioFilter{.frequency = 8000}, AudioFilter{.frequency = 16000},
  };

  EXPECT_CALL(
      *dispatcher,
      SendEvent(AllOf(
          Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::ApplyAudioFilters),
          Field(&interface::CustomEvent::content, VariantWith<EqualizerPreset>(audio_filters)))));

  // Apply EQ
  block->OnEvent(ftxui::Event::Character('a'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│                                                                                             │
│╭─────────────╮                                                           ▂▂                 │
││→ Custom     │           ▇▇                                              ██                 │
│╰─────────────╯   ██      ██      ██      ▆▆      ██      ▄▄      ██      ██     ██     ██   │
│                  ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
│                                                                                             │
│                  0 dB    5 dB    0 dB   -2 dB    0 dB   -3 dB    0 dB    7 dB    0 dB   0 dB│
│                                                                                             │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, ModifyEqualizerAndReset) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Change 250Hz frequency (using keybindings for frequency navigation)
  std::string typed{"lllllkkkkk"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│                                                                                             │
│╭─────────────╮                                                                              │
││→ Custom     │                           ▇▇                                                 │
│╰─────────────╯   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
│                  ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
│                                                                                             │
│                  0 dB    0 dB    0 dB    5 dB    0 dB    0 dB    0 dB    0 dB    0 dB   0 dB│
│                                                                                             │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation to check that will not send any audio filters
  EXPECT_CALL(
      *dispatcher,
      SendEvent(AllOf(
          Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::ApplyAudioFilters),
          Field(&interface::CustomEvent::content, VariantWith<model::EqualizerPreset>(_)))))
      .Times(0);

  // Reset EQ
  block->OnEvent(ftxui::Event::Character('r'));

  // And try to apply EQ
  block->OnEvent(ftxui::Event::Character('a'));

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│                                                                                             │
│╭─────────────╮                                                                              │
││→ Custom     │                                                                              │
│╰─────────────╯   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
│                  ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
│                                                                                             │
│                  0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB   0 dB│
│                                                                                             │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, SelectOtherPresetAndApply) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Using keybindings for navigation, open preset picker
  std::string typed{"l jj"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│╭─────────────╮                                                                              │
││↓ Custom     │ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│├─────────────┤                                                                              │
││◉ Custom     │                                                                              │
││○ Electronic │                                                                              │
││○ Pop        │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││○ Rock       │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││             │                                                                              │
││             │   0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB   0 dB│
│╰─────────────╯                                                                              │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation to check that will send audio filters matching Electronic EQ
  using model::AudioFilter;
  using model::EqualizerPreset;
  EqualizerPreset audio_filters{AudioFilter::CreatePresets()["Electronic"]};

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ApplyAudioFilters),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::EqualizerPreset>(audio_filters)))));

  // Select and apply Electronic EQ
  typed = " a";
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│╭─────────────╮                                                                              │
││↓ Electronic │ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│├─────────────┤                                                                              │
││○ Custom     │                                                                              │
││◉ Electronic │   ▃▃      ▄▄      ▃▃                      ▂▂      ▄▄      ▂▂     ▃▃     ▃▃   │
││○ Pop        │   ██      ██      ██      ▆▆      ██      ██      ██      ██     ██     ██   │
││○ Rock       │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││             │                                                                              │
││             │   2 dB    3 dB    2 dB   -2 dB    0 dB    1 dB    3 dB    1 dB    2 dB   2 dB│
│╰─────────────╯                                                                              │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, AttemptToModifyFixedPreset) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Setup expectation to check that will send audio filters matching Pop EQ
  using model::AudioFilter;
  using model::EqualizerPreset;
  EqualizerPreset audio_filters{AudioFilter::CreatePresets()["Pop"]};

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ApplyAudioFilters),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::EqualizerPreset>(audio_filters)))));

  // Using keybindings for navigation, open preset picker, select and apply "Pop"
  std::string typed{"l jjj a"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│╭─────────────╮                                                                              │
││↓ Pop        │ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│├─────────────┤                                                                              │
││○ Custom     │                                                                              │
││○ Electronic │   ▂▂      ▃▃      ▂▂                      ▃▃      ▂▂      ▂▂     ▃▃     ▄▄   │
││◉ Pop        │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││○ Rock       │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││             │                                                                              │
││             │   1 dB    2 dB    1 dB    0 dB    0 dB    2 dB    1 dB    1 dB    2 dB   3 dB│
│╰─────────────╯                                                                              │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation to check that will not send any event to update audio filters
  EXPECT_CALL(
      *dispatcher,
      SendEvent(AllOf(
          Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::ApplyAudioFilters),
          Field(&interface::CustomEvent::content, VariantWith<model::EqualizerPreset>(_)))))
      .Times(0);

  // Attempt to modify some frequency bars and apply
  typed = "llkkljllkka";
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│╭─────────────╮                                                                              │
││↓ Pop        │ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│├─────────────┤                                                                              │
││○ Custom     │                                                                              │
││○ Electronic │   ▂▂      ▃▃      ▂▂                      ▃▃      ▂▂      ▂▂     ▃▃     ▄▄   │
││◉ Pop        │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││○ Rock       │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││             │                                                                              │
││             │   1 dB    2 dB    1 dB    0 dB    0 dB    2 dB    1 dB    1 dB    2 dB   3 dB│
│╰─────────────╯                                                                              │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, AttemptToResetFixedPreset) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Setup expectation to check that will send audio filters matching Pop EQ
  using model::AudioFilter;
  using model::EqualizerPreset;
  EqualizerPreset audio_filters{AudioFilter::CreatePresets()["Rock"]};

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ApplyAudioFilters),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::EqualizerPreset>(audio_filters)))));

  // Using keybindings for navigation, open preset picker, select and apply "Rock"
  std::string typed{"l jjjj a"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│╭─────────────╮                                                                              │
││↓ Rock       │ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│├─────────────┤                                                                              │
││○ Custom     │                                                                              │
││○ Electronic │   ▂▂      ▃▃      ▂▂                                      ▂▂     ▃▃     ▄▄   │
││○ Pop        │   ██      ██      ██      ▇▇      ▄▄      ▇▇      ██      ██     ██     ██   │
││◉ Rock       │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││             │                                                                              │
││             │   1 dB    2 dB    1 dB   -1 dB   -3 dB   -1 dB    0 dB    1 dB    2 dB   3 dB│
│╰─────────────╯                                                                              │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation to check that will not send any event to update audio filters
  EXPECT_CALL(
      *dispatcher,
      SendEvent(AllOf(
          Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::ApplyAudioFilters),
          Field(&interface::CustomEvent::content, VariantWith<model::EqualizerPreset>(_)))))
      .Times(0);

  // Attempt to reset EQ
  block->OnEvent(ftxui::Event::Character('r'));

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│╭─────────────╮                                                                              │
││↓ Rock       │ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│├─────────────┤                                                                              │
││○ Custom     │                                                                              │
││○ Electronic │   ▂▂      ▃▃      ▂▂                                      ▂▂     ▃▃     ▄▄   │
││○ Pop        │   ██      ██      ██      ▇▇      ▄▄      ▇▇      ██      ██     ██     ██   │
││◉ Rock       │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││             │                                                                              │
││             │   1 dB    2 dB    1 dB   -1 dB   -3 dB   -1 dB    0 dB    1 dB    2 dB   3 dB│
│╰─────────────╯                                                                              │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, ModifyEqualizerChangePresetAndSwitchback) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Change some frequencies (using keybindings for frequency navigation)
  std::string typed{"lllkkkkklljjlljjjllkkkkkkk"};
  utils::QueueCharacterEvents(*block, typed);

  // Setup expectation for event with new audio filters applied
  using model::AudioFilter;
  using model::EqualizerPreset;

  auto all_presets = AudioFilter::CreatePresets();

  EqualizerPreset audio_filters{
      AudioFilter{.frequency = 32},   AudioFilter{.frequency = 64, .gain = 5},
      AudioFilter{.frequency = 125},  AudioFilter{.frequency = 250, .gain = -2},
      AudioFilter{.frequency = 500},  AudioFilter{.frequency = 1000, .gain = -3},
      AudioFilter{.frequency = 2000}, AudioFilter{.frequency = 4000, .gain = 7},
      AudioFilter{.frequency = 8000}, AudioFilter{.frequency = 16000},
  };

  EXPECT_CALL(
      *dispatcher,
      SendEvent(AllOf(
          Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::ApplyAudioFilters),
          Field(&interface::CustomEvent::content, VariantWith<EqualizerPreset>(audio_filters)))));

  // Apply EQ
  block->OnEvent(ftxui::Event::Character('a'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│                                                                                             │
│╭─────────────╮                                                           ▂▂                 │
││→ Custom     │           ▇▇                                              ██                 │
│╰─────────────╯   ██      ██      ██      ▆▆      ██      ▄▄      ██      ██     ██     ██   │
│                  ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
│                                                                                             │
│                  0 dB    5 dB    0 dB   -2 dB    0 dB   -3 dB    0 dB    7 dB    0 dB   0 dB│
│                                                                                             │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Focus genre picker and change preset to "Electronic"
  block->OnEvent(ftxui::Event::Escape);

  // Setup expectation to check that will send audio filters matching Electronic EQ
  EqualizerPreset electronic_preset{all_presets["Electronic"]};

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ApplyAudioFilters),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::EqualizerPreset>(electronic_preset)))));

  typed = "l jj a";
  utils::QueueCharacterEvents(*block, typed);

  // It is necessary to clear screen, otherwise it will be dirty
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│╭─────────────╮                                                                              │
││↓ Electronic │ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│├─────────────┤                                                                              │
││○ Custom     │                                                                              │
││◉ Electronic │   ▃▃      ▄▄      ▃▃                      ▂▂      ▄▄      ▂▂     ▃▃     ▃▃   │
││○ Pop        │   ██      ██      ██      ▆▆      ██      ██      ██      ██     ██     ██   │
││○ Rock       │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││             │                                                                              │
││             │   2 dB    3 dB    2 dB   -2 dB    0 dB    1 dB    3 dB    1 dB    2 dB   2 dB│
│╰─────────────╯                                                                              │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation for event with new audio filters applied
  EXPECT_CALL(
      *dispatcher,
      SendEvent(AllOf(
          Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::ApplyAudioFilters),
          Field(&interface::CustomEvent::content, VariantWith<EqualizerPreset>(audio_filters)))));

  // Switchback to "Custom" preset
  typed = "k a";
  utils::QueueCharacterEvents(*block, typed);

  // It is necessary to clear screen, otherwise it will be dirty
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│╭─────────────╮                                                                              │
││↓ Custom     │ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz 16 kHz │
│├─────────────┤                                                                              │
││◉ Custom     │                                                           ▂▂                 │
││○ Electronic │           ▇▇                                              ██                 │
││○ Pop        │   ██      ██      ██      ▆▆      ██      ▄▄      ██      ██     ██     ██   │
││○ Rock       │   ██      ██      ██      ██      ██      ██      ██      ██     ██     ██   │
││             │                                                                              │
││             │   0 dB    5 dB    0 dB   -2 dB    0 dB   -3 dB    0 dB    7 dB    0 dB   0 dB│
│╰─────────────╯                                                                              │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, FetchSongLyrics) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                     No song playing...                                      │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  auto finder = GetFinder();

  std::string expected_artist{"Deko"};
  std::string expected_title{"Midnight Tokyo"};

  // Setup expectations before start fetching song lyrics
  EXPECT_CALL(*finder, Search(expected_artist, expected_title))
      .WillOnce(Invoke([](const std::string&, const std::string&) {
        // Wait a bit, to simulate execution of Finder async task
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        return lyric::SongLyric{
            "Found crazy lyrics\n"
            "about some stuff\n"
            "that I don't even know\n",
        };
      }));

  // Send event to notify that song has started playing
  model::Song audio{
      .filepath = "/path/to/song.mp3",
      .artist = "Deko",
      .title = "Midnight Tokyo",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 193,
  };

  auto event_update_song = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update_song);

  // It is necessary to clear screen, otherwise it will be dirty
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                     Fetching lyrics...                                      │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Wait a bit, just until Finder async task finishes its execution
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // It is necessary to clear screen, otherwise it will be dirty
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                   Found crazy lyrics                                        │
│                                   about some stuff                                          │
│                                   that I don't even know                                    │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, FetchSongLyricsFailed) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto finder = GetFinder();

  std::string expected_artist{"southstar"};
  std::string expected_title{"Miss You"};

  // Setup expectations before start fetching song lyrics
  EXPECT_CALL(*finder, Search(expected_artist, expected_title))
      .WillOnce(Invoke([](const std::string&, const std::string&) {
        // Wait a bit, to simulate execution of Finder async task
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        return lyric::SongLyric{};
      }));

  // Send event to notify that song has started playing
  model::Song audio{
      .filepath = "/path/to/song.mp3",
      .artist = "southstar",
      .title = "Miss You",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 193,
  };

  auto event_update_song = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update_song);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                     Fetching lyrics...                                      │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Wait a bit, just until Finder async task finishes its execution
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // It is necessary to clear screen, otherwise it will be dirty
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                     Failed to fetch =(                                      │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, FetchSongLyricsWithoutMetadata) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto finder = GetFinder();

  std::string expected_artist{"NiteWind"};
  std::string expected_title{"Lucid Memories"};

  // Setup expectations before start fetching song lyrics
  EXPECT_CALL(*finder, Search(expected_artist, expected_title))
      .WillOnce(Invoke([](const std::string&, const std::string&) {
        // Wait a bit, to simulate execution of Finder async task
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        return lyric::SongLyric{
            "Funny you asked\n"
            "Yeah, found something\n",
        };
      }));

  // Send event to notify that song has started playing
  model::Song audio{
      .filepath = "/contains/some/huge/path/NiteWind-Lucid Memories.mp3",
      .artist = "",
      .title = "",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 193,
  };

  auto event_update_song = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update_song);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                     Fetching lyrics...                                      │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Wait for Finder async task to finish it
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // It is necessary to clear screen, otherwise it will be dirty
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                   Funny you asked                                           │
│                                   Yeah, found something                                     │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, FetchSongLyricsWithDifferentFilenames) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto finder = GetFinder();

  auto setup_expectation_for_find = [&](const std::string& filepath,
                                        const std::string& expected_artist,
                                        const std::string& expected_title, int times = 1) {
    if (times == 0)
      // Setup expectations before start fetching song lyrics
      EXPECT_CALL(*finder, Search(expected_artist, expected_title)).Times(0);
    else
      // Setup expectations before start fetching song lyrics
      EXPECT_CALL(*finder, Search(expected_artist, expected_title))
          .WillRepeatedly(Return(lyric::SongLyric{}));

    // Send event to notify that song has started playing
    model::Song audio{.filepath = filepath};

    auto event_update_song = interface::CustomEvent::UpdateSongInfo(audio);
    Process(event_update_song);

    // Wait for Finder async task to finish it
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  };

  // Attempt 1 - ok
  setup_expectation_for_find("yatashigang- BREATHE.mp4", "yatashigang", "BREATHE");

  // Attempt 2 - ok
  setup_expectation_for_find("yatashigang  -BREATHE.mp4", "yatashigang", "BREATHE");

  // Attempt 3 - nok
  setup_expectation_for_find("yatashigang BREATHE.mp4", "", "", 0);

  // Attempt 4 - nok
  setup_expectation_for_find("yatashigang-BREATHE", "", "", 0);

  // Attempt 5 - nok
  setup_expectation_for_find("yatashigang=BREATHE.mp3", "", "", 0);

  // Attempt 6 - ok
  setup_expectation_for_find("yatashigang-BREATHE .mp4", "yatashigang", "BREATHE");
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, FetchSongLyricsAndClear) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto finder = GetFinder();

  std::string expected_artist{"Joey Bada$$"};
  std::string expected_title{"Show Me"};

  // Setup expectations before start fetching song lyrics
  EXPECT_CALL(*finder, Search(expected_artist, expected_title))
      .WillOnce(Invoke([](const std::string&, const std::string&) {
        // Wait a bit, to simulate execution of Finder async task
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        return lyric::SongLyric{
            "Just imagine the lyrics\n"
            "In this block\n",
        };
      }));

  // Send event to notify that song has started playing
  model::Song audio{.filepath = "/contains/Joey Bada$$-Show Me.mp3"};

  auto event_update_song = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update_song);

  // Wait for Finder async task to finish it
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                  Just imagine the lyrics                                    │
│                                  In this block                                              │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Send event to clear song info
  auto event_clear = interface::CustomEvent::ClearSongInfo();
  Process(event_clear);

  // It is necessary to clear screen, otherwise it will be dirty
  screen->Clear();

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                     No song playing...                                      │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, FetchScrollableSongLyrics) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto finder = GetFinder();

  std::string expected_artist{"Rüfüs Du Sol"};
  std::string expected_title{"Innerbloom"};

  // Setup expectations before start fetching song lyrics
  EXPECT_CALL(*finder, Search(expected_artist, expected_title))
      .WillOnce(Invoke([](const std::string&, const std::string&) {
        // Wait a bit, to simulate execution of Finder async task
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        return lyric::SongLyric{
            "Feels like I'm waiting\n"
            "Like I'm watching\n"
            "Watching you for love\n"
            "Dreams, where I am fading\n"
            "Fading\n",

            "So free my mind\n"
            "All the talking\n"
            "Wasting all your time\n"
            "I'm giving all\n"
            "That I've got\n",

            "Feels like I'm dreaming\n"
            "Like I'm walking\n"
            "Walking by your side\n"
            "Keeps on repeating\n"
            "Repeating\n",

            "So free my mind\n"
            "All the talking\n"
            "Wasting all your time\n"
            "I'm giving all\n"
            "That I've got\n",

            "If you want me\n"
            "If you need me\n"
            "I'm yours\n",

            "If you want me\n"
            "If you need me\n"
            "I'm yours\n",

            "If you want me\n"
            "If you need me\n"
            "I'm yours\n",

            "If you want me\n"
            "If you need me\n"
            "I'm yours\n",

            "If you want me\n"
            "If you need me\n"
            "I'm yours\n",

            "If you want me\n"
            "If you need me\n"
            "I'm yours\n",
        };
      }));

  // Send event to notify that song has started playing
  model::Song audio{.filepath = "Rüfüs Du Sol-Innerbloom.mp3"};

  auto event_update_song = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update_song);

  // Wait for Finder async task to finish it
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                 Feels like I'm waiting                                     ┃│
│                                 Like I'm watching                                          ┃│
│                                 Watching you for love                                      ┃│
│                                 Dreams, where I am fading                                  ┃│
│                                 Fading                                                      │
│                                                                                             │
│                                 So free my mind                                             │
│                                 All the talking                                             │
│                                 Wasting all your time                                       │
│                                 I'm giving all                                              │
│                                 That I've got                                               │
│                                                                                             │
│                                 Feels like I'm dreaming                                     │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Scroll lyrics
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowUp);
  block->OnEvent(ftxui::Event::Character('j'));
  block->OnEvent(ftxui::Event::Character('j'));

  // Clear screen and render again to get updated lyrics
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                 Feels like I'm dreaming                                     │
│                                 Like I'm walking                                            │
│                                 Walking by your side                                        │
│                                 Keeps on repeating                                         ┃│
│                                 Repeating                                                  ┃│
│                                                                                            ┃│
│                                 So free my mind                                            ┃│
│                                 All the talking                                             │
│                                 Wasting all your time                                       │
│                                 I'm giving all                                              │
│                                 That I've got                                               │
│                                                                                             │
│                                 If you want me                                              │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Scroll to the end
  block->OnEvent(ftxui::Event::End);

  // Clear screen and render again to get updated lyrics
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                 If you want me                                              │
│                                 If you need me                                              │
│                                 I'm yours                                                   │
│                                                                                             │
│                                 If you want me                                              │
│                                 If you need me                                              │
│                                 I'm yours                                                   │
│                                                                                             │
│                                 If you want me                                             ┃│
│                                 If you need me                                             ┃│
│                                 I'm yours                                                  ┃│
│                                                                                            ┃│
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Scroll back to the begin
  block->OnEvent(ftxui::Event::Home);

  // Clear screen and render again to get updated lyrics
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                 Feels like I'm waiting                                     ┃│
│                                 Like I'm watching                                          ┃│
│                                 Watching you for love                                      ┃│
│                                 Dreams, where I am fading                                  ┃│
│                                 Fading                                                      │
│                                                                                             │
│                                 So free my mind                                             │
│                                 All the talking                                             │
│                                 Wasting all your time                                       │
│                                 I'm giving all                                              │
│                                 That I've got                                               │
│                                                                                             │
│                                 Feels like I'm dreaming                                     │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, FetchSongLyricsOnBackground) {
  auto finder = GetFinder();

  std::string expected_artist{"The Virgins"};
  std::string expected_title{"Rich Girls"};

  // Setup expectations before start fetching song lyrics
  EXPECT_CALL(*finder, Search(expected_artist, expected_title))
      .WillOnce(Invoke([](const std::string&, const std::string&) {
        // Wait a bit, to simulate execution of Finder async task
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        return lyric::SongLyric{
            "Funny you asked\n"
            "Yeah, found something\n",
        };
      }));

  // Send event to notify that song has started playing
  model::Song audio{
      .filepath = "/contains/some/huge/path/The Virgins-Rich Girls.mp3",
      .artist = "",
      .title = "",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 193,
  };

  auto event_update_song = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update_song);

  // Wait for Finder async task to finish it
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                   Funny you asked                                           │
│                                   Yeah, found something                                     │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

/**
 * @brief Tests with TabViewer mock class (just to test focus)
 */
class MockTabViewerTest : public ::BlockTest {
  //! Create mock class from TabViewer
  class TabViewerMock final : public interface::TabViewer {
   public:
    using TabViewer::TabViewer;

    MOCK_METHOD(void, OnFocus, (), (override));
    MOCK_METHOD(void, OnLostFocus, (), (override));
  };

 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(95, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create TabViewer block
    block = ftxui::Make<TabViewerMock>(dispatcher);
  }

  //! Getter for mock
  auto GetMock() -> TabViewerMock* {
    // Return tab viewer mock
    return static_cast<TabViewerMock*>(block.get());
  }
};

TEST_F(MockTabViewerTest, CheckFocus) {
  auto tabviewer_mock = GetMock();

  EXPECT_CALL(*tabviewer_mock, OnFocus());
  tabviewer_mock->SetFocused(true);

  EXPECT_CALL(*tabviewer_mock, OnLostFocus());
  tabviewer_mock->SetFocused(false);

  // Expect block to send an event asking for focus on block
  EXPECT_CALL(
      *dispatcher,
      SendEvent(
          AllOf(Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::SetFocused),
                Field(&interface::CustomEvent::content,
                      VariantWith<model::BlockIdentifier>(model::BlockIdentifier::TabViewer)))))
      .WillOnce(Invoke([&](const interface::CustomEvent&) {
        // Simulate terminal behavior
        tabviewer_mock->SetFocused(true);
      }));

  // Set focus on tab item 1
  EXPECT_CALL(*tabviewer_mock, OnFocus());
  block->OnEvent(ftxui::Event::Character('1'));

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(22, 0.001));
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ──────────────────────────────────────────[F1:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│   ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁   │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace
