#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include "general/block.h"
#include "general/utils.h"
#include "mock/event_dispatcher_mock.h"
#include "mock/lyric_finder_mock.h"
#include "view/block/main_content.h"
#include "view/block/main_content/song_lyric.h"
#include "view/element/flash_message.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::VariantWith;

/**
 * @brief Tests with MainContent class
 */
class MainContentTest : public ::BlockTest {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(95, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create MainContent block
    block = ftxui::Make<interface::MainContent>(dispatcher);

    // Set this block as focused
    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);

    // As we dot want to use dependency injection for Tabview::SongLyrics, we will override
    // LyricFinder manually...  First of all, get tab viewer
    auto block_main_tab = static_cast<interface::MainContent*>(block.get());

    // Then get song lyric tab item
    auto song_lyric = static_cast<interface::SongLyric*>(
        block_main_tab->tab_elem_[interface::MainContent::View::Lyric].get());

    // And finally, override lyric finder to use a mock
    song_lyric->finder_ = std::make_unique<LyricFinderMock>();
  }

  //! Getter for LyricFinder (necessary as inner variable is an unique_ptr)
  auto GetFinder() -> LyricFinderMock* {
    // Get tab viewer
    auto main_tab = static_cast<interface::MainContent*>(block.get());

    // Get song lyric tab item
    auto song_lyric = static_cast<interface::SongLyric*>(
        main_tab->tab_elem_[interface::MainContent::View::Lyric].get());

    // Return lyric finder mock
    return static_cast<LyricFinderMock*>(song_lyric->finder_.get());
  }

  static constexpr int kNumberBars = 30;  //!< Number of bars for visualizer tab view
};

/* ********************************************************************************************** */

TEST_F(MainContentTest, InitialRender) {
  auto event_bars =
      interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(kNumberBars, 0.001));
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
│  ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, AnimationHorizontalMirror) {
  std::vector<double> values{
      0.99, 0.90, 0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.28, 0.24, 0.20, 0.15, 0.09, 0.06, 0.03,
      0.99, 0.90, 0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.28, 0.24, 0.20, 0.15, 0.09, 0.06, 0.03,
  };

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                            ▇▇ ▇▇                                            │
│                                         ▆▆ ██ ██ ▆▆                                         │
│                                      ▅▅ ██ ██ ██ ██ ▅▅                                      │
│                                   ▃▃ ██ ██ ██ ██ ██ ██ ▃▃                                   │
│                                   ██ ██ ██ ██ ██ ██ ██ ██                                   │
│                                ██ ██ ██ ██ ██ ██ ██ ██ ██ ██                                │
│                             ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▇▇                             │
│                          ▃▃ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▃▃                          │
│                       ▃▃ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▃▃                       │
│                 ▁▁ ▆▆ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▆▆ ▁▁                 │
│              ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▅▅              │
│        ▂▂ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▂▂        │
│  ▄▄ ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▇▇ ▄▄  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, AnimationVerticalMirror) {
  std::vector<double> values{
      0.1,  0.2, 0.3,  0.4, 0.5,  0.4, 0.3,  0.2,  0.1,  0.2,  0.3,  0.4,  0.5,  0.55, 0.6,
      0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 0.90, 0.85, 0.80, 0.75, 0.70, 0.65, 0.60, 0.55,

      0.1,  0.2, 0.3,  0.4, 0.5,  0.4, 0.3,  0.2,  0.1,  0.2,  0.3,  0.4,  0.5,  0.55, 0.6,
      0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 0.90, 0.85, 0.80, 0.75, 0.70, 0.65, 0.60, 0.55,
  };

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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                           ▁▁ ▄▄ ▆▆ ▄▄ ▁▁     Vertical mirror│
│                                                  ▂▂ ▄▄ ▇▇ ██ ██ ██ ██ ██ ▇▇ ▄▄ ▂▂           │
│                                         ▃▃ ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▅▅ ▃▃  │
│           ▄▄ ██ ▄▄                ▄▄ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██  │
│     ▂▂ ▇▇ ██ ██ ██ ▇▇ ▂▂    ▂▂ ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██  │
│  ▅▅ ██ ██ ██ ██ ██ ██ ██ ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██  │
│  ▃▃                      ▃▃                                                                 │
│  ██ ▅▅                ▅▅ ██ ▅▅                                                              │
│  ██ ██ ██ ▂▂    ▂▂ ██ ██ ██ ██ ██ ▂▂                                                        │
│  ██ ██ ██ ██ ▄▄ ██ ██ ██ ██ ██ ██ ██ ▄▄ ▂▂                                              ▂▂  │
│  ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▇▇ ▄▄ ▁▁                            ▁▁ ▄▄ ▇▇ ██  │
│  ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▆▆ ▄▄ ▁▁          ▁▁ ▄▄ ▆▆ ██ ██ ██ ██  │
│  ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▆▆ ▃▃ ▆▆ ██ ██ ██ ██ ██ ██ ██  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, AnimationMono) {
  std::vector<double> values{
      0.1, 0.2,  0.3, 0.4, 0.5, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.2, 0.25, 0.3, 0.35,
      0.4, 0.45, 0.5, 0.6, 0.7, 0.8, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3,  0.2, 0.1,

      0.1, 0.2,  0.3, 0.4, 0.5, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.2, 0.25, 0.3, 0.35,
      0.4, 0.45, 0.5, 0.6, 0.7, 0.8, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3,  0.2, 0.1,
  };

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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                                                         Mono│
│                                                                 ▆▆                          │
│                                                              ▄▄ ██ ▄▄                       │
│                                                           ▁▁ ██ ██ ██ ▁▁                    │
│                                                           ██ ██ ██ ██ ██                    │
│                 ▇▇                                     ▇▇ ██ ██ ██ ██ ██ ▇▇                 │
│              ▄▄ ██ ▄▄                               ▄▄ ██ ██ ██ ██ ██ ██ ██ ▄▄              │
│           ▂▂ ██ ██ ██ ▂▂                      ▂▂ ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▂▂           │
│           ██ ██ ██ ██ ██                   ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██           │
│        ██ ██ ██ ██ ██ ██ ██          ▂▂ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██        │
│     ▅▅ ██ ██ ██ ██ ██ ██ ██ ▅▅    ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▅▅     │
│  ▃▃ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▃▃ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▃▃  │
│  ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, IncreaseAndDecreaseBarWidth) {
  std::vector<double> values{
      0.75, 0.70, 0.65, 0.60, 0.55, 0.50, 0.45, 0.40, 0.35, 0.30, 0.25, 0.20, 0.15, 0.10, 0.06,
      0.75, 0.70, 0.65, 0.60, 0.55, 0.50, 0.45, 0.40, 0.35, 0.30, 0.25, 0.20, 0.15, 0.10, 0.06,
  };

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                         ▁▁ ▆▆ ▆▆ ▁▁                                         │
│                                      ▄▄ ██ ██ ██ ██ ▄▄                                      │
│                                ▂▂ ▇▇ ██ ██ ██ ██ ██ ██ ▇▇ ▂▂                                │
│                             ▄▄ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▄▄                             │
│                       ▂▂ ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▇▇ ▂▂                       │
│                    ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▅▅                    │
│              ▂▂ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▂▂              │
│           ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▅▅           │
│     ▃▃ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▃▃     │
│  ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▇▇  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectations (it will call twice once because of internal min-max values)
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::UpdateBarWidth)))
      .Times(2);

  // Increase bar width
  block->OnEvent(ftxui::Event::Character('.'));
  block->OnEvent(ftxui::Event::Character('.'));
  block->OnEvent(ftxui::Event::Character('.'));

  values = std::vector<double>{
      0.45, 0.40, 0.35, 0.30, 0.25, 0.20, 0.15, 0.10, 0.06,
      0.45, 0.40, 0.35, 0.30, 0.25, 0.20, 0.15, 0.10, 0.06,
  };

  event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  // Clear screen and render again
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                     ▂▂▂▂ ▇▇▇▇ ▇▇▇▇ ▂▂▂▂                                     │
│                                ▅▅▅▅ ████ ████ ████ ████ ▅▅▅▅                                │
│                      ▂▂▂▂ ████ ████ ████ ████ ████ ████ ████ ████ ▂▂▂▂                      │
│                 ▅▅▅▅ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ▅▅▅▅                 │
│       ▃▃▃▃ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ▃▃▃▃       │
│  ▇▇▇▇ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ████ ▇▇▇▇  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectations (it will call two times because of internal min-max values)
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::UpdateBarWidth)));

  // Decrease bar width
  block->OnEvent(ftxui::Event::Character(','));

  values = std::vector<double>{
      0.60, 0.48, 0.40, 0.35, 0.30, 0.24, 0.20, 0.15, 0.10, 0.06, 0.02,
      0.60, 0.48, 0.40, 0.35, 0.30, 0.24, 0.20, 0.15, 0.10, 0.06, 0.02,
  };

  event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  // Clear screen and render again
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                           ▇▇▇ ▇▇▇                                           │
│                                       ▂▂▂ ███ ███ ▂▂▂                                       │
│                                   ▂▂▂ ███ ███ ███ ███ ▂▂▂                                   │
│                               ▅▅▅ ███ ███ ███ ███ ███ ███ ▅▅▅                               │
│                       ▁▁▁ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▁▁▁                       │
│                   ▅▅▅ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▅▅▅                   │
│           ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▃▃▃           │
│   ▃▃▃ ▇▇▇ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▇▇▇ ▃▃▃   │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, ToggleFullscreenOnVisualizer) {
  // Navigation keys must not toggle fullscreen, only the dedicated keybinding
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::ToggleFullscreen)));

  block->OnEvent(ftxui::Event::Character('h'));
  block->OnEvent(ftxui::Event::Character('z'));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, VisualizerOnFullscreen) {
  auto tab_viewer = std::static_pointer_cast<interface::MainContent>(block);

  // Send audio data to show on visualizer
  std::vector<double> values{
      0.99, 0.93, 0.87, 0.81, 0.75, 0.69, 0.63, 0.57,
      0.51, 0.45, 0.39, 0.33, 0.27, 0.21, 0.15, 0.09,

      0.99, 0.93, 0.87, 0.81, 0.75, 0.69, 0.63, 0.57,
      0.51, 0.45, 0.39, 0.33, 0.27, 0.21, 0.15, 0.09,
  };

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  // Render block as fullscreen
  ftxui::Render(*screen, tab_viewer->RenderFullscreen());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
                                             ▇▇ ▇▇       z: exit fullscreen · Horizontal mirror
                                       ▁▁ ██ ██ ██ ██ ▁▁                                       
                                    ▂▂ ██ ██ ██ ██ ██ ██ ▂▂                                    
                                 ▂▂ ██ ██ ██ ██ ██ ██ ██ ██ ▂▂                                 
                              ▃▃ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▃▃                              
                           ▄▄ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▄▄                           
                        ▅▅ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▅▅                        
                     ▆▆ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▆▆                     
                  ▆▆ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▆▆                  
               ▇▇ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▇▇               
         ▁▁ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▁▁         
      ▂▂ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▂▂      
   ▂▂ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▂▂   
▃▃ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ▃▃
██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Make sure that tab_viewer will not change active tab item while on fullscreen
  block->OnEvent(ftxui::Event::Character('2'));

  // Render again
  screen->Clear();
  ftxui::Render(*screen, tab_viewer->RenderFullscreen());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  // And check that screen is equal to before
  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, HideFullscreenHintAfterExit) {
  auto tab_viewer = std::static_pointer_cast<interface::MainContent>(block);

  // Entering fullscreen shows a hint on how to exit it
  ftxui::Render(*screen, tab_viewer->RenderFullscreen());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("z: exit fullscreen · Horizontal mirror"));

  // After exiting fullscreen, hint must not be shown anymore
  screen->Clear();
  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, Not(HasSubstr("exit fullscreen")));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, RenderEqualizer) {
  block->OnEvent(ftxui::Event::Character('2'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, ModifyEqualizerAndApply) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Change 64Hz frequency (using keybindings for frequency navigation)
  std::string typed{"llkkkkk"};
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, ModifyEqualizerAndReset) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Change 250Hz frequency (using keybindings for frequency navigation)
  std::string typed{"llllkkkkk"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, SelectOtherPresetAndApply) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Using keybindings for navigation, open preset picker
  std::string typed{"lh jj"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, CyclePresetsWithClosedPicker) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // First navigation key focuses the first frequency bar, so go back to focus the preset picker
  std::string typed{"lh"};
  utils::QueueCharacterEvents(*block, typed);

  // Without opening the picker, go to next preset
  block->OnEvent(ftxui::Event::Character('j'));

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("→ Electronic"));

  // Go back twice, which must wrap around to the last preset
  block->OnEvent(ftxui::Event::ArrowUp);
  block->OnEvent(ftxui::Event::Character('k'));

  // Setup expectation to check that will send audio filters matching Rock EQ
  using model::AudioFilter;
  using model::EqualizerPreset;
  EqualizerPreset audio_filters{AudioFilter::CreatePresets()["Rock"]};

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ApplyAudioFilters),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::EqualizerPreset>(audio_filters)))));

  block->OnEvent(ftxui::Event::Character('a'));

  screen->Clear();
  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("→ Rock"));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, AttemptToModifyFixedPreset) {
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
  std::string typed{"lh jjj a"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, AttemptToResetFixedPreset) {
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
  std::string typed{"lh jjjj a"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, ModifyEqualizerChangePresetAndSwitchback) {
  // Set focus on tab item 2
  block->OnEvent(ftxui::Event::Character('2'));

  // Change some frequencies (using keybindings for frequency navigation)
  std::string typed{"llkkkkklljjlljjjllkkkkkkk"};
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

  typed = "lh jj a";
  utils::QueueCharacterEvents(*block, typed);

  // It is necessary to clear screen, otherwise it will be dirty
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, FetchSongLyrics) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

        return model::SongLyric{
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, FetchSongLyricsFailed) {
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

        return model::SongLyric{};
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, FetchSongLyricsWithoutMetadata) {
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

        return model::SongLyric{
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, FetchSongLyricsWithDifferentFilenames) {
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
          .WillRepeatedly(Return(model::SongLyric{}));

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

TEST_F(MainContentTest, FetchSongLyricsAndClear) {
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

        return model::SongLyric{
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, FetchScrollableSongLyrics) {
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

        return model::SongLyric{
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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

TEST_F(MainContentTest, FetchSongLyricsOnBackground) {
  auto finder = GetFinder();

  std::string expected_artist{"The Virgins"};
  std::string expected_title{"Rich Girls"};

  // Setup expectations before start fetching song lyrics
  EXPECT_CALL(*finder, Search(expected_artist, expected_title))
      .WillOnce(Invoke([](const std::string&, const std::string&) {
        // Wait a bit, to simulate execution of Finder async task
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        return model::SongLyric{
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
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
 * @brief Tests with MainContent mock class (just to test focus)
 */
class MockMainContentTest : public ::BlockTest {
  //! Create mock class from MainContent
  class MainContentMock final : public interface::MainContent {
   public:
    using MainContent::MainContent;

    MOCK_METHOD(void, OnFocus, (), (override));
    MOCK_METHOD(void, OnLostFocus, (), (override));
  };

 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(95, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create MainContent block
    block = ftxui::Make<MainContentMock>(dispatcher);
  }

  //! Getter for mock
  auto GetMock() -> MainContentMock* {
    // Return tab viewer mock
    return static_cast<MainContentMock*>(block.get());
  }
};

TEST_F(MockMainContentTest, CheckFocus) {
  auto main_content_mock = GetMock();

  EXPECT_CALL(*main_content_mock, OnFocus());
  main_content_mock->SetFocused(true);

  EXPECT_CALL(*main_content_mock, OnLostFocus());
  main_content_mock->SetFocused(false);

  // Expect block to send an event asking for focus on block
  EXPECT_CALL(
      *dispatcher,
      SendEvent(
          AllOf(Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::SetFocused),
                Field(&interface::CustomEvent::content,
                      VariantWith<model::BlockIdentifier>(model::BlockIdentifier::MainContent)))))
      .WillOnce(Invoke([&](const interface::CustomEvent&) {
        // Simulate terminal behavior
        main_content_mock->SetFocused(true);
      }));

  // Set focus on tab item 1
  EXPECT_CALL(*main_content_mock, OnFocus());
  block->OnEvent(ftxui::Event::Character('1'));

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(30, 0.001));
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
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
│  ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁ ▁▁  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

/**
 * @brief Tests with FlashMessage class
 */
class FlashMessageTest : public ::testing::Test {
 protected:
  static constexpr std::chrono::milliseconds kDuration{50};   //!< Time that message stays visible
  static constexpr std::chrono::milliseconds kTimeout{2000};  //!< Maximum time to wait for events

  void SetUp() override {
    message = std::make_unique<interface::FlashMessage>(
        [this] {
          expired++;
          if (!notified.exchange(true)) {
            on_expire.set_value();
          }
        },
        kDuration);
  }

  //! Wait until expiration callback is triggered (or timeout)
  bool WaitForExpiration() {
    return on_expire.get_future().wait_for(kTimeout) == std::future_status::ready;
  }

  std::unique_ptr<interface::FlashMessage> message;  //!< Flash message under test

  std::atomic<int> expired = 0;        //!< Number of times that message expired
  std::atomic<bool> notified = false;  //!< Control to set promise only once
  std::promise<void> on_expire;        //!< Notify test that message expired
};

/* ********************************************************************************************** */

TEST_F(FlashMessageTest, InitialState) { EXPECT_FALSE(message->GetText().has_value()); }

/* ********************************************************************************************** */

TEST_F(FlashMessageTest, ShowAndExpire) {
  message->Show("Mono");
  EXPECT_THAT(message->GetText(), Optional(Eq("Mono")));

  ASSERT_TRUE(WaitForExpiration());

  EXPECT_FALSE(message->GetText().has_value());
  EXPECT_EQ(expired, 1);
}

/* ********************************************************************************************** */

TEST_F(FlashMessageTest, ReplaceMessage) {
  message->Show("Mono");
  message->Show("Vertical mirror");

  EXPECT_THAT(message->GetText(), Optional(Eq("Vertical mirror")));

  ASSERT_TRUE(WaitForExpiration());

  // Even with two messages shown, only the last one expires
  EXPECT_FALSE(message->GetText().has_value());
  EXPECT_EQ(expired, 1);
}

/* ********************************************************************************************** */

TEST_F(FlashMessageTest, HideBeforeExpiration) {
  message->Show("Mono");
  message->Hide();

  EXPECT_FALSE(message->GetText().has_value());

  // Wait longer than message duration, callback must not be triggered for a hidden message
  std::this_thread::sleep_for(kDuration * 3);
  EXPECT_EQ(expired, 0);
}

/* ********************************************************************************************** */

TEST_F(FlashMessageTest, DestroyWhileVisible) {
  message->Show("Mono");
  message.reset();

  EXPECT_EQ(expired, 0);
}

}  // namespace
