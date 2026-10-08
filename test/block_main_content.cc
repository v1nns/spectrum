#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include "audio/lyric/lyric_finder.h"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/node.hpp"
#include "general/block.h"
#include "general/utils.h"
#include "mock/event_dispatcher_mock.h"
#include "mock/file_handler_mock.h"
#include "mock/lyric_finder_mock.h"
#include "model/bar_animation.h"
#include "model/song.h"
#include "view/block/main_content.h"
#include "view/block/main_content/audio_equalizer.h"
#include "view/block/main_content/song_lyric.h"
#include "view/element/flash_message.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrEq;
using ::testing::VariantWith;

//! Create search result for lyric finder mock (lyrics found, unless they are empty)
lyric::SearchResult MakeSearchResult(model::SongLyric lyrics) {
  auto status =
      lyrics.empty() ? lyric::SearchResult::Status::NotFound : lyric::SearchResult::Status::Found;

  return lyric::SearchResult{.status = status, .lyrics = std::move(lyrics)};
}

/* ********************************************************************************************** */

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

    // Create MainContent block (using a mock to not load/save settings from user's home)
    block = ftxui::Make<interface::MainContent>(dispatcher, file_handler);

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

    // Song lyrics are fetched in another thread, which asks for UI refresh when result is ready
    EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                             interface::CustomEvent::Identifier::Refresh)))
        .Times(AnyNumber());
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

  //! Select animation using picker: open it, move selection until animation and keep it
  void SelectAnimation(model::BarAnimation animation) {
    block->OnEvent(ftxui::Event::Character('a'));
    for (int i = model::BarAnimation::HorizontalMirror; i < animation; i++) {
      block->OnEvent(ftxui::Event::Character('j'));
    }
    block->OnEvent(ftxui::Event::Return);
  }

  //! Getter for AudioEqualizer tab item
  auto GetEqualizer() -> interface::AudioEqualizer* {
    auto main_tab = static_cast<interface::MainContent*>(block.get());
    return static_cast<interface::AudioEqualizer*>(
        main_tab->tab_elem_[interface::MainContent::View::Equalizer].get());
  }

  //! Check if frequency bar (from AudioEqualizer) is focused
  bool IsFrequencyBarFocused(int index) { return GetEqualizer()->bars_[index].IsFocused(); }

  //! Getter for frequency bar gain (from AudioEqualizer)
  double GetFrequencyBarGain(int index) { return GetEqualizer()->bars_[index].filter->gain; }

  //! Getter for frequency bar box (from AudioEqualizer), only valid after rendering block
  ftxui::Box GetFrequencyBarBox(int index) { return GetEqualizer()->bars_[index].Box(); }

  //! Render block until song lyrics are no longer being fetched (or timeout) and return screen
  std::string RenderUntilFetched() {
    constexpr std::chrono::milliseconds kTimeout{2000};
    constexpr std::chrono::milliseconds kInterval{5};

    std::string rendered;

    for (std::chrono::milliseconds elapsed{0}; elapsed < kTimeout; elapsed += kInterval) {
      screen->Clear();
      ftxui::Render(*screen, block->Render());
      rendered = utils::FilterAnsiCommands(screen->ToString());

      if (rendered.find("Fetching lyrics...") == std::string::npos) {
        break;
      }

      std::this_thread::sleep_for(kInterval);
    }

    return rendered;
  }

  static constexpr int kNumberBars = 30;  //!< Number of bars for visualizer tab view

  //! Load/save settings (by default, there are no settings saved)
  std::shared_ptr<NiceMock<FileHandlerMock>> file_handler =
      std::make_shared<NiceMock<FileHandlerMock>>();
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

TEST_F(MainContentTest, AnimationMonoUsesAverageFromChannels) {
  // Left channel is silent, while right channel is at maximum height
  constexpr int kBarsPerChannel = 30;
  std::vector<double> values(kBarsPerChannel, 0.0);
  values.insert(values.end(), kBarsPerChannel, 1.0);

  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());

  SelectAnimation(model::BarAnimation::Mono);

  Process(interface::CustomEvent::DrawAudioSpectrum(values));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  // Bars must be drawn with half of their maximum height (average from both channels)
  int rows_with_bars = 0;
  std::istringstream lines{rendered};
  for (std::string line; std::getline(lines, line);) {
    if (line.find("█") != std::string::npos) {
      rows_with_bars++;
    }
  }

  // Block has 13 rows for content, so half of them must contain bars (allowing one row of rounding)
  constexpr double kHalfContentRows = 13 / 2.0;
  constexpr double kTolerance = 1.0;
  EXPECT_NEAR(rows_with_bars, kHalfContentRows, kTolerance) << rendered;
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, AnimationSpectrumLine) {
  // Single line, using the average from both channels
  // Left channel: rising then falling values, right channel: the same values in reverse order
  std::vector<double> left{0.1, 0.2, 0.4, 0.7, 0.9, 0.7, 0.4, 0.2, 0.1, 0.1,
                           0.2, 0.3, 0.5, 0.6, 0.5, 0.3, 0.2, 0.1, 0.1, 0.1};
  std::vector<double> values(left);
  values.insert(values.end(), left.rbegin(), left.rend());

  // Ignore any event sent while changing animation
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());

  // Select animation using picker
  SelectAnimation(model::BarAnimation::SpectrumLine);

  Process(interface::CustomEvent::DrawAudioSpectrum(values));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  const std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                  ⣠⠞⠉⠉⠉⠉⠙⠲⢤⣀                                     ⢀⣠⠴⠚⠉⠉⠉⠉⠙⢦⡀                 │
│               ⣀⡴⠋⠁        ⠈⠙⠲⣄                               ⢀⡴⠚⠉         ⠉⠳⣄⡀              │
│             ⣠⠞⠁              ⠈⠙⢦⣀                         ⢀⣠⠞⠉               ⠙⢦⡀            │
│           ⢀⡼⠁                   ⠈⠳⣄⡀                    ⣀⡴⠋                    ⠹⣄           │
│         ⣠⠴⠋                        ⠙⢦⣀               ⢀⣠⠞⠁                       ⠈⠳⢤⡀        │
│     ⣀⡤⠖⠋⠁                            ⠈⠓⠲⠤⢤⣀⣀⣀⣀⣀⣀⣀⣠⠤⠴⠒⠋                             ⠉⠓⠦⣄⡀    │
│⠤⠖⠒⠋⠉⠁                                                                                  ⠉⠉⠓⠒⠦│
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, AnimationSpectrumLineMirror) {
  // Left channel above the middle and right channel below it
  // Left channel: rising then falling values, right channel: the same values in reverse order
  std::vector<double> left{0.1, 0.2, 0.4, 0.7, 0.9, 0.7, 0.4, 0.2, 0.1, 0.1,
                           0.2, 0.3, 0.5, 0.6, 0.5, 0.3, 0.2, 0.1, 0.1, 0.1};
  std::vector<double> values(left);
  values.insert(values.end(), left.rbegin(), left.rend());

  // Ignore any event sent while changing animation
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());

  // Select animation using picker
  SelectAnimation(model::BarAnimation::SpectrumLineMirror);

  Process(interface::CustomEvent::DrawAudioSpectrum(values));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  const std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                   ⣀⣀                                                                        │
│               ⣀⡤⠖⠋⠁⠈⠙⠲⢤⣀                                                                    │
│            ⢀⡤⠞⠁        ⠈⠳⢤⡀                                 ⢀⣀⣀⣀⣀                           │
│          ⣠⠴⠋              ⠙⠦⣄                          ⢀⣠⠖⠚⠉⠉   ⠈⠉⠙⠒⠦⣄                      │
│      ⢀⣠⠖⠋⠁                  ⠈⠙⠦⣄⡀                 ⢀⣀⡤⠴⠚⠉             ⠈⠙⠲⠤⣄⣀                 │
│⣀⣀⡤⠤⠖⠚⠉                          ⠉⠓⠲⠤⢤⣀⣀⣀⣀⣀⣀⣀⣀⡤⠴⠒⠚⠉⠉                       ⠈⠉⠙⠒⠲⠤⢤⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀│
│                                                                                             │
│⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠒⠦⠤⣄⣀⡀                          ⣀⣀⡤⠤⠖⠒⠒⠒⠒⠒⠒⠒⠲⠤⢤⣀⡀                           ⢀⣀⡤⠤⠖⠒│
│                ⠉⠉⠓⠲⢤⣀               ⢀⡤⠖⠒⠋⠉⠁               ⠉⠓⠦⣄⡀                   ⢀⣠⠴⠚⠉     │
│                     ⠈⠙⠲⢤⣀⣀     ⢀⣀⣠⠴⠚⠉                         ⠉⠳⣄⡀             ⢀⣠⠞⠉         │
│                          ⠈⠉⠙⠒⠚⠉⠉                                 ⠙⠦⣄         ⣀⡴⠋            │
│                                                                    ⠈⠙⠲⢤⣀ ⢀⡤⠖⠋⠁              │
│                                                                        ⠈⠉⠉                  │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, AnimationSpectrumLineFilled) {
  // Same as single line, but with the area below it filled
  // Left channel: rising then falling values, right channel: the same values in reverse order
  std::vector<double> left{0.1, 0.2, 0.4, 0.7, 0.9, 0.7, 0.4, 0.2, 0.1, 0.1,
                           0.2, 0.3, 0.5, 0.6, 0.5, 0.3, 0.2, 0.1, 0.1, 0.1};
  std::vector<double> values(left);
  values.insert(values.end(), left.rbegin(), left.rend());

  // Ignore any event sent while changing animation
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());

  // Select animation using picker
  SelectAnimation(model::BarAnimation::SpectrumLineFilled);

  Process(interface::CustomEvent::DrawAudioSpectrum(values));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  const std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                                                                                             │
│                  ▄██████▙▄▖                                     ▗▄▟██████▄                  │
│               ▄▟████████████▙▖                               ▗▟████████████▙▄               │
│             ▄██████████████████▄▖                         ▗▄██████████████████▄             │
│           ▗▟█████████████████████▙▄                     ▄▟█████████████████████▙▖           │
│         ▄▟██████████████████████████▄▖               ▗▄██████████████████████████▙▄         │
│     ▄▄█████████████████████████████████▙▄▄▄▄▄▄▄▄▄▄▄▟█████████████████████████████████▄▄     │
│▄███████████████████████████████████████████████████████████████████████████████████████████▄│
│█████████████████████████████████████████████████████████████████████████████████████████████│
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, PickAnimationWithPreview) {
  // Opening picker does not change animation
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::ChangeBarAnimation)))
      .Times(0);
  block->OnEvent(ftxui::Event::Character('a'));

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│╭ animation ─────────────────────╮                                                           │
││▶ Horizontal mirror             │                                                           │
││  Vertical mirror               │                                                           │
││  Mono                          │                                                           │
││  Horizontal mirror (no space)  │                                                           │
││  Vertical mirror (no space)    │                                                           │
││  Mono (no space)               │                                                           │
││  Line                          │                                                           │
││  Line (mirror)                 │                                                           │
││  Line (filled)                 │                                                           │
││  Line (filled mirror)          │                                                           │
│╰────────────────────────────────╯                                                           │
│                                                                                             │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // There is nothing above first animation
  block->OnEvent(ftxui::Event::Character('k'));

  // Moving selection changes animation right away
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(
                  Field(&interface::CustomEvent::id,
                        interface::CustomEvent::Identifier::ChangeBarAnimation),
                  Field(&interface::CustomEvent::content,
                        VariantWith<model::BarAnimation>(model::BarAnimation::VerticalMirror)))));
  block->OnEvent(ftxui::Event::Character('j'));

  ftxui::Render(*screen, block->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), HasSubstr("▶ Vertical mirror"));

  // Keeping it closes picker and saves it
  EXPECT_CALL(*file_handler,
              SaveSettings(AllOf(
                  Field(&model::Settings::animation, Optional(model::BarAnimation::VerticalMirror)),
                  Field(&model::Settings::bar_width, Optional(2)))))
      .WillOnce(Return(true));
  block->OnEvent(ftxui::Event::Return);

  screen->Clear();
  ftxui::Render(*screen, block->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), Not(HasSubstr("▶")));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, PickAnimationWithMouse) {
  using interface::CustomEvent;

  //! Create event for mouse button released at the position where the given text is rendered
  auto mouse_at = [this](const std::string& text, ftxui::Mouse::Button button) {
    for (int y = 0; y < screen->dimy(); y++) {
      for (int x = 0; x + static_cast<int>(text.size()) <= screen->dimx(); x++) {
        bool found = true;
        for (size_t i = 0; i < text.size() && found; i++) {
          found = screen->PixelAt(x + static_cast<int>(i), y).character == text.substr(i, 1);
        }

        if (found) {
          return ftxui::Event::Mouse(
              "", ftxui::Mouse{.button = button, .motion = ftxui::Mouse::Released, .x = x, .y = y});
        }
      }
    }

    ADD_FAILURE() << "Text not found on screen: " << text;
    return ftxui::Event::Custom;
  };

  auto animation_changed_to = [](model::BarAnimation animation) {
    return AllOf(Field(&CustomEvent::id, CustomEvent::Identifier::ChangeBarAnimation),
                 Field(&CustomEvent::content, VariantWith<model::BarAnimation>(animation)));
  };

  block->OnEvent(ftxui::Event::Character('a'));
  ftxui::Render(*screen, block->Render());

  // Wheel moves selection by one animation, changing it right away
  EXPECT_CALL(*dispatcher, SendEvent(animation_changed_to(model::BarAnimation::VerticalMirror)));
  EXPECT_TRUE(block->OnEvent(mouse_at("Line (mirror)", ftxui::Mouse::WheelDown)));

  // Click selects the animation under mouse cursor
  EXPECT_CALL(*dispatcher, SendEvent(animation_changed_to(model::BarAnimation::SpectrumLine)));
  EXPECT_TRUE(block->OnEvent(mouse_at("Line  ", ftxui::Mouse::Left)));

  ftxui::Render(*screen, block->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), HasSubstr("▶ Line  "));

  // Double-click keeps it, closing picker and saving it
  EXPECT_CALL(*dispatcher, SendEvent(animation_changed_to(model::BarAnimation::Mono)));
  EXPECT_CALL(*file_handler,
              SaveSettings(Field(&model::Settings::animation, Optional(model::BarAnimation::Mono))))
      .WillOnce(Return(true));
  EXPECT_TRUE(block->OnEvent(mouse_at("Mono  ", ftxui::Mouse::Left)));

  screen->Clear();
  ftxui::Render(*screen, block->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), Not(HasSubstr("▶")));

  // Without picker, mouse is not handled by visualizer
  auto outside = ftxui::Event::Mouse("", ftxui::Mouse{.button = ftxui::Mouse::WheelDown,
                                                      .motion = ftxui::Mouse::Pressed,
                                                      .x = screen->dimx() / 2,
                                                      .y = screen->dimy() / 2});
  EXPECT_FALSE(block->OnEvent(outside));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, CancelAnimationPicker) {
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());
  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  block->OnEvent(ftxui::Event::Character('a'));
  block->OnEvent(ftxui::Event::Character('j'));
  block->OnEvent(ftxui::Event::ArrowDown);

  // Going back restores animation from before opening picker
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(
                  Field(&interface::CustomEvent::id,
                        interface::CustomEvent::Identifier::ChangeBarAnimation),
                  Field(&interface::CustomEvent::content,
                        VariantWith<model::BarAnimation>(model::BarAnimation::HorizontalMirror)))));
  block->OnEvent(ftxui::Event::Escape);

  ftxui::Render(*screen, block->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), Not(HasSubstr("▶")));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, RestoreAndSaveVisualizerSettings) {
  // Saved settings are restored when block is created (invalid bar width is ignored)
  EXPECT_CALL(*file_handler, ParseSettings(_))
      .WillOnce(DoAll(
          SetArgReferee<0>(model::Settings{.animation = model::BarAnimation::Mono, .bar_width = 3}),
          Return(true)))
      .WillOnce(DoAll(SetArgReferee<0>(model::Settings{.bar_width = 99}), Return(true)));

  auto restored = ftxui::Make<interface::MainContent>(dispatcher, file_handler);
  auto main_content = std::static_pointer_cast<interface::MainContent>(restored);
  main_content->SetFocused(true);
  EXPECT_EQ(main_content->GetBarWidth(), 3);

  restored->OnEvent(ftxui::Event::Character('a'));
  ftxui::Render(*screen, restored->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), HasSubstr("▶ Mono"));

  auto invalid = std::static_pointer_cast<interface::MainContent>(
      ftxui::Make<interface::MainContent>(dispatcher, file_handler));
  EXPECT_EQ(invalid->GetBarWidth(), 2);

  // Changing bar width saves it
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::bar_width, Optional(3))))
      .WillOnce(Return(true));
  block->OnEvent(ftxui::Event::Character('.'));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, AnimationSpectrumLineFilledMirror) {
  // Same as mirrored lines, but with the area between middle and each line filled
  // Left channel: rising then falling values, right channel: the same values in reverse order
  std::vector<double> left{0.1, 0.2, 0.4, 0.7, 0.9, 0.7, 0.4, 0.2, 0.1, 0.1,
                           0.2, 0.3, 0.5, 0.6, 0.5, 0.3, 0.2, 0.1, 0.1, 0.1};
  std::vector<double> values(left);
  values.insert(values.end(), left.rbegin(), left.rend());

  // Ignore any event sent while changing animation
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());

  // Select animation using picker
  SelectAnimation(model::BarAnimation::SpectrumLineFilledMirror);

  Process(interface::CustomEvent::DrawAudioSpectrum(values));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  const std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                   ▄▖                                                                        │
│               ▄▄█████▙▄▖                                                                    │
│            ▗▄███████████▙▄                                  ▗▄▄▄▖                           │
│          ▄▟████████████████▄▖                          ▗▄███████████▄▖                      │
│      ▗▄███████████████████████▄▄                  ▗▄▄▟█████████████████▙▄▄▖                 │
│▄▄▄▄███████████████████████████████▙▄▄▄▄▄▄▄▄▄▄▄▟███████████████████████████████▙▄▄▄▄▄▄▄▄▄▄▄▄▄│
│█████████████████████████████████████████████████████████████████████████████████████████████│
│▀▀▀▀▀▀▀▀▀▀▀▀███████████████████████████████████▀▀▀▀▀▀▀▀▜███████████████████████████████████▀▀│
│                ▀▀▀▜███████████████████▀▀▀▀                ▀▀████████████████████████▛▀▘     │
│                     ▝▀▜███████████▛▀▘                         ▀▜█████████████████▀▘         │
│                          ▝▀▀▀▀▀▘                                 ▀████████████▛▘            │
│                                                                    ▝▀▜█████▀▀               │
│                                                                        ▝▀▘                  │
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

  // Expect block to send an event to terminal when animation is selected
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(
                  Field(&interface::CustomEvent::id,
                        interface::CustomEvent::Identifier::ChangeBarAnimation),
                  Field(&interface::CustomEvent::content,
                        VariantWith<model::BarAnimation>(model::BarAnimation::VerticalMirror)))));

  SelectAnimation(model::BarAnimation::VerticalMirror);

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  // Maybe filtering ansi commands is messing up with this animation =(
  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                           ▁▁ ▄▄ ▆▆ ▄▄ ▁▁                    │
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

  // Expect block to send an event to terminal for each animation selected in picker
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

  SelectAnimation(model::BarAnimation::Mono);

  // Send event to fill internal data to use it later for rendering animation
  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                                                             │
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

TEST_F(MainContentTest, CalculateNumberOfBarsWhileVisualizerIsNotActive) {
  // Focus equalizer tab
  block->OnEvent(ftxui::Event::Character('2'));

  // Terminal was resized, so visualizer must ask for a new number of bars even if it is not active
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ResizeAnalysis),
                              Field(&interface::CustomEvent::content, VariantWith<int>(22)))));

  Process(interface::CustomEvent::CalculateNumberOfBars(22));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, RenderEqualizer) {
  block->OnEvent(ftxui::Event::Character('2'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer  3:lyric ─────────────────────────────────────────[F12:help]───[X]╮
│                                                                                             │
│                      32     64    125    250    500     1k     2k     4k     8k     16k     │
│                                                                                             │
│   ╭─────────────╮                                                                           │
│   │→ Custom     │                                                                           │
│   ╰─────────────╯    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│                      ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│                                                                                             │
│                      0      0      0      0      0      0      0      0      0       0      │
│                                                                                             │
│                               ┌─────────────┐┌─────────────┐                                │
│                               │    Apply    ││    Reset    │                                │
│                               └─────────────┘└─────────────┘                                │
╰─────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, RenderEqualizerWithEnoughSpace) {
  // With enough width, labels contain their units
  screen = std::make_unique<ftxui::Screen>(140, 15);

  block->OnEvent(ftxui::Event::Character('2'));

  ftxui::Render(*screen, block->Render());
  const std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("32 Hz"));
  EXPECT_THAT(rendered, HasSubstr("16 kHz"));
  EXPECT_THAT(rendered, HasSubstr("0 dB"));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, ClickOnEqualizerFocusesBlockAndBand) {
  // Show equalizer, then simulate another block taking focus
  block->OnEvent(ftxui::Event::Character('2'));
  std::static_pointer_cast<interface::Block>(block)->SetFocused(false);

  // Render block to calculate position of each element on screen
  ftxui::Render(*screen, block->Render());

  constexpr int kBand = 1;
  ftxui::Box box = GetFrequencyBarBox(kBand);
  ASSERT_FALSE(IsFrequencyBarFocused(kBand));

  // Clicking on equalizer must ask for focus, so keys go to it afterwards
  EXPECT_CALL(
      *dispatcher,
      SendEvent(
          AllOf(Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::SetFocused),
                Field(&interface::CustomEvent::content,
                      VariantWith<model::BlockIdentifier>(model::BlockIdentifier::MainContent)))));

  ftxui::Mouse mouse{.button = ftxui::Mouse::Left,
                     .motion = ftxui::Mouse::Released,
                     .x = box.x_min,
                     .y = box.y_min};

  EXPECT_TRUE(block->OnEvent(ftxui::Event::Mouse("", mouse)));

  // And clicked band is the one focused (instead of any band focused before)
  EXPECT_TRUE(IsFrequencyBarFocused(kBand));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, MouseWheelOnEqualizerBand) {
  // Show equalizer and render block to calculate position of each element on screen
  block->OnEvent(ftxui::Event::Character('2'));
  ftxui::Render(*screen, block->Render());

  constexpr int kBand = 2;
  ftxui::Box box = GetFrequencyBarBox(kBand);
  double gain = GetFrequencyBarGain(kBand);

  ftxui::Mouse mouse{.button = ftxui::Mouse::WheelUp,
                     .motion = ftxui::Mouse::Pressed,
                     .x = box.x_min,
                     .y = box.y_min};

  // Scroll up twice and down once
  EXPECT_TRUE(block->OnEvent(ftxui::Event::Mouse("", mouse)));
  EXPECT_TRUE(block->OnEvent(ftxui::Event::Mouse("", mouse)));

  mouse.button = ftxui::Mouse::WheelDown;
  EXPECT_TRUE(block->OnEvent(ftxui::Event::Mouse("", mouse)));

  EXPECT_DOUBLE_EQ(GetFrequencyBarGain(kBand), gain + 1);
  EXPECT_TRUE(IsFrequencyBarFocused(kBand));
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
│                      32     64    125    250    500     1k     2k     4k     8k     16k     │
│                                                                                             │
│   ╭─────────────╮                                                     ▂▂                    │
│   │→ Custom     │           ▇▇                                        ██                    │
│   ╰─────────────╯    ██     ██     ██     ▆▆     ██     ▄▄     ██     ██     ██      ██     │
│                      ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│                                                                                             │
│                      0      5      0      -2     0      -3     0      7      0       0      │
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
│                      32     64    125    250    500     1k     2k     4k     8k     16k     │
│                                                                                             │
│   ╭─────────────╮                                                                           │
│   │→ Custom     │                         ▇▇                                                │
│   ╰─────────────╯    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│                      ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│                                                                                             │
│                      0      0      0      5      0      0      0      0      0       0      │
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
│                      32     64    125    250    500     1k     2k     4k     8k     16k     │
│                                                                                             │
│   ╭─────────────╮                                                                           │
│   │→ Custom     │                                                                           │
│   ╰─────────────╯    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│                      ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│                                                                                             │
│                      0      0      0      0      0      0      0      0      0       0      │
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
│   ╭─────────────╮                                                                           │
│   │↓ Custom     │    32     64    125    250    500     1k     2k     4k     8k     16k     │
│   ├─────────────┤                                                                           │
│   │◉ Custom     │                                                                           │
│   │○ Electronic │                                                                           │
│   │○ Pop        │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │○ Rock       │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │             │                                                                           │
│   │             │    0      0      0      0      0      0      0      0      0       0      │
│   ╰─────────────╯                                                                           │
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
│   ╭─────────────╮                                                                           │
│   │↓ Electronic │    32     64    125    250    500     1k     2k     4k     8k     16k     │
│   ├─────────────┤                                                                           │
│   │○ Custom     │                                                                           │
│   │◉ Electronic │    ▃▃     ▄▄     ▃▃                   ▂▂     ▄▄     ▂▂     ▃▃      ▃▃     │
│   │○ Pop        │    ██     ██     ██     ▆▆     ██     ██     ██     ██     ██      ██     │
│   │○ Rock       │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │             │                                                                           │
│   │             │    2      3      2      -2     0      1      3      1      2       2      │
│   ╰─────────────╯                                                                           │
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
│   ╭─────────────╮                                                                           │
│   │↓ Pop        │    32     64    125    250    500     1k     2k     4k     8k     16k     │
│   ├─────────────┤                                                                           │
│   │○ Custom     │                                                                           │
│   │○ Electronic │    ▂▂     ▃▃     ▂▂                   ▃▃     ▂▂     ▂▂     ▃▃      ▄▄     │
│   │◉ Pop        │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │○ Rock       │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │             │                                                                           │
│   │             │    1      2      1      0      0      2      1      1      2       3      │
│   ╰─────────────╯                                                                           │
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
│   ╭─────────────╮                                                                           │
│   │↓ Pop        │    32     64    125    250    500     1k     2k     4k     8k     16k     │
│   ├─────────────┤                                                                           │
│   │○ Custom     │                                                                           │
│   │○ Electronic │    ▂▂     ▃▃     ▂▂                   ▃▃     ▂▂     ▂▂     ▃▃      ▄▄     │
│   │◉ Pop        │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │○ Rock       │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │             │                                                                           │
│   │             │    1      2      1      0      0      2      1      1      2       3      │
│   ╰─────────────╯                                                                           │
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
│   ╭─────────────╮                                                                           │
│   │↓ Rock       │    32     64    125    250    500     1k     2k     4k     8k     16k     │
│   ├─────────────┤                                                                           │
│   │○ Custom     │                                                                           │
│   │○ Electronic │    ▂▂     ▃▃     ▂▂                                 ▂▂     ▃▃      ▄▄     │
│   │○ Pop        │    ██     ██     ██     ▇▇     ▄▄     ▇▇     ██     ██     ██      ██     │
│   │◉ Rock       │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │             │                                                                           │
│   │             │    1      2      1      -1     -3     -1     0      1      2       3      │
│   ╰─────────────╯                                                                           │
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
│   ╭─────────────╮                                                                           │
│   │↓ Rock       │    32     64    125    250    500     1k     2k     4k     8k     16k     │
│   ├─────────────┤                                                                           │
│   │○ Custom     │                                                                           │
│   │○ Electronic │    ▂▂     ▃▃     ▂▂                                 ▂▂     ▃▃      ▄▄     │
│   │○ Pop        │    ██     ██     ██     ▇▇     ▄▄     ▇▇     ██     ██     ██      ██     │
│   │◉ Rock       │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │             │                                                                           │
│   │             │    1      2      1      -1     -3     -1     0      1      2       3      │
│   ╰─────────────╯                                                                           │
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
│                      32     64    125    250    500     1k     2k     4k     8k     16k     │
│                                                                                             │
│   ╭─────────────╮                                                     ▂▂                    │
│   │→ Custom     │           ▇▇                                        ██                    │
│   ╰─────────────╯    ██     ██     ██     ▆▆     ██     ▄▄     ██     ██     ██      ██     │
│                      ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│                                                                                             │
│                      0      5      0      -2     0      -3     0      7      0       0      │
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
│   ╭─────────────╮                                                                           │
│   │↓ Electronic │    32     64    125    250    500     1k     2k     4k     8k     16k     │
│   ├─────────────┤                                                                           │
│   │○ Custom     │                                                                           │
│   │◉ Electronic │    ▃▃     ▄▄     ▃▃                   ▂▂     ▄▄     ▂▂     ▃▃      ▃▃     │
│   │○ Pop        │    ██     ██     ██     ▆▆     ██     ██     ██     ██     ██      ██     │
│   │○ Rock       │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │             │                                                                           │
│   │             │    2      3      2      -2     0      1      3      1      2       2      │
│   ╰─────────────╯                                                                           │
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
│   ╭─────────────╮                                                                           │
│   │↓ Custom     │    32     64    125    250    500     1k     2k     4k     8k     16k     │
│   ├─────────────┤                                                                           │
│   │◉ Custom     │                                                     ▂▂                    │
│   │○ Electronic │           ▇▇                                        ██                    │
│   │○ Pop        │    ██     ██     ██     ▆▆     ██     ▄▄     ██     ██     ██      ██     │
│   │○ Rock       │    ██     ██     ██     ██     ██     ██     ██     ██     ██      ██     │
│   │             │                                                                           │
│   │             │    0      5      0      -2     0      -3     0      7      0       0      │
│   ╰─────────────╯                                                                           │
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

        return MakeSearchResult(model::SongLyric{
            "Found crazy lyrics\n"
            "about some stuff\n"
            "that I don't even know\n",
        });
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

        return MakeSearchResult(model::SongLyric{});
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
│                                    Lyrics not found for                                     │
│                                   "southstar - Miss You"                                    │
│                                                                                             │
│                                      r: retry search                                        │
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

        return MakeSearchResult(model::SongLyric{
            "Funny you asked\n"
            "Yeah, found something\n",
        });
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

TEST_F(MainContentTest, RetryFetchSongLyricsAfterNetworkError) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto* finder = GetFinder();

  // First attempt fails to reach search engines, second one finds song lyrics
  EXPECT_CALL(*finder, Search(std::string{"southstar"}, std::string{"Miss You"}))
      .WillOnce(Return(lyric::SearchResult{.status = lyric::SearchResult::Status::FetchFailed}))
      .WillOnce(Return(MakeSearchResult(model::SongLyric{"Miss you, miss you\n"})));

  const model::Song audio{
      .filepath = "/path/to/song.mp3", .artist = "southstar", .title = "Miss You"};
  Process(interface::CustomEvent::UpdateSongInfo(audio));

  std::string rendered = RenderUntilFetched();
  EXPECT_THAT(rendered, HasSubstr("Could not reach lyrics websites (network error)"));
  EXPECT_THAT(rendered, HasSubstr("r: retry search"));

  // Retry search
  block->OnEvent(ftxui::Event::Character('r'));

  rendered = RenderUntilFetched();
  EXPECT_THAT(rendered, HasSubstr("Miss you, miss you"));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, FetchSongLyricsWithoutArtistAndTitle) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto* finder = GetFinder();

  // Without metadata and without "artist - title" pattern in filename, search is not possible
  EXPECT_CALL(*finder, Search(_, _)).Times(0);

  const model::Song audio{.filepath = "/path/to/song.mp3"};
  Process(interface::CustomEvent::UpdateSongInfo(audio));

  const std::string rendered = RenderUntilFetched();
  EXPECT_THAT(rendered, HasSubstr("Cannot search lyrics without artist and title"));
  EXPECT_THAT(rendered, Not(HasSubstr("retry")));

  // Retry is not possible, as it would fail for the same reason
  EXPECT_FALSE(block->OnEvent(ftxui::Event::Character('r')));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, FetchSongLyricsFromStream) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto* finder = GetFinder();

  // Streamed song has no filepath, artist and title come from parsing its title
  EXPECT_CALL(*finder, Search(std::string{"Sonic Youth"}, std::string{"Kool Thing"}))
      .WillOnce(Return(MakeSearchResult(model::SongLyric{"I don't wanna, I don't think so\n"})));

  const model::Song audio{
      .artist = "Sonic Youth",
      .title = "Kool Thing",
      .stream_info = model::StreamInfo{.base_url = "https://www.youtube.com/watch?v=dummy"},
  };
  Process(interface::CustomEvent::UpdateSongInfo(audio));

  const std::string rendered = RenderUntilFetched();
  EXPECT_THAT(rendered, HasSubstr("I don't wanna, I don't think so"));
}

/* ********************************************************************************************** */

TEST_F(MainContentTest, FetchSongLyricsFromStreamWithoutArtist) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto* finder = GetFinder();

  // Stream title was not in "artist - title" format, so only title is known
  EXPECT_CALL(*finder, Search(_, _)).Times(0);

  const model::Song audio{
      .title = "so be it",
      .stream_info = model::StreamInfo{.base_url = "https://www.youtube.com/watch?v=dummy"},
  };
  Process(interface::CustomEvent::UpdateSongInfo(audio));

  const std::string rendered = RenderUntilFetched();
  EXPECT_THAT(rendered, HasSubstr("Cannot search lyrics without artist and title"));
  EXPECT_THAT(rendered, HasSubstr("video title is not in the \"Artist - Title\" format"));
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
          .WillRepeatedly(Return(lyric::SearchResult{}));

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

        return MakeSearchResult(model::SongLyric{
            "Just imagine the lyrics\n"
            "In this block\n",
        });
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

TEST_F(MainContentTest, ChangeSongWhileFetchingLyrics) {
  // Set focus on tab item 3
  block->OnEvent(ftxui::Event::Character('3'));

  auto finder = GetFinder();

  // Search for first song gets stuck (e.g. slow network) until it is released
  std::promise<void> release;
  std::shared_future<void> released = release.get_future().share();
  std::atomic<bool> first_started = false;

  EXPECT_CALL(*finder, Search("Artist A", "Song A"))
      .WillOnce(Invoke([&](const std::string&, const std::string&) {
        first_started = true;
        released.wait();
        return MakeSearchResult(model::SongLyric{"Lyrics from first song\n"});
      }));

  EXPECT_CALL(*finder, Search("Artist B", "Song B"))
      .WillOnce(Return(MakeSearchResult(model::SongLyric{"Lyrics from second song\n"})));

  Process(interface::CustomEvent::UpdateSongInfo(
      model::Song{.filepath = "/a.mp3", .artist = "Artist A", .title = "Song A"}));

  for (int i = 0; i < 200 && !first_started; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  ASSERT_TRUE(first_started);

  // Changing song must not wait for the search in progress
  auto change = std::async(std::launch::async, [this] {
    Process(interface::CustomEvent::ClearSongInfo());
    Process(interface::CustomEvent::UpdateSongInfo(
        model::Song{.filepath = "/b.mp3", .artist = "Artist B", .title = "Song B"}));
  });

  bool changed_without_waiting =
      change.wait_for(std::chrono::seconds(1)) == std::future_status::ready;

  // Always release first search, so this test never hangs
  release.set_value();
  change.wait();

  EXPECT_TRUE(changed_without_waiting);

  // Result from first search is discarded
  std::string rendered = RenderUntilFetched();
  EXPECT_THAT(rendered, HasSubstr("Lyrics from second song"));
  EXPECT_THAT(rendered, Not(HasSubstr("Lyrics from first song")));
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

        return MakeSearchResult(model::SongLyric{
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
        });
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

        return MakeSearchResult(model::SongLyric{
            "Funny you asked\n"
            "Yeah, found something\n",
        });
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

    // Create MainContent block (using a mock to not load/save settings from user's home)
    block = ftxui::Make<MainContentMock>(dispatcher, std::make_shared<NiceMock<FileHandlerMock>>());
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

/* ********************************************************************************************** */

TEST_F(MainContentTest, ChangeThemeAfterCreation) {
  utils::ThemeGuard guard;

  const auto window_button = utils::MarkerColor(1);
  const auto bar = utils::MarkerColor(2);
  const auto button = utils::MarkerColor(3);
  const auto all = {window_button, bar, button};

  // Show audio equalizer and keep mouse over exit button (last one on block border), as window
  // buttons have no color in normal state
  block->OnEvent(ftxui::Event::Character('2'));
  ftxui::Render(*screen, block->Render());

  const std::string border = utils::FilterAnsiCommands(screen->ToString());
  const auto exit_button =
      static_cast<int>(ftxui::string_width(border.substr(1, border.find("[X]"))));

  ftxui::Mouse mouse{
      .button = ftxui::Mouse::None, .motion = ftxui::Mouse::Released, .x = exit_button, .y = 0};
  block->OnEvent(ftxui::Event::Mouse("", mouse));

  // Block was created with default theme
  ftxui::Render(*screen, block->Render());
  for (const auto& color : all) EXPECT_FALSE(utils::HasColor(*screen, color));

  // Replace theme, the same block must use new colors on next render
  interface::Theme theme;
  theme.block.window_button = utils::AllButtonStates(window_button);
  theme.equalizer.bar = interface::Theme::State{.foreground = bar, .background = bar};
  theme.equalizer.button = utils::AllButtonStates(button);
  interface::SetTheme(theme);

  ftxui::Render(*screen, block->Render());
  for (const auto& color : all) EXPECT_TRUE(utils::HasColor(*screen, color));
}

}  // namespace
