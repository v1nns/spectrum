#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT

#include "general/block.h"
#include "general/utils.h"  // for FilterAnsiCommands
#include "mock/event_dispatcher_mock.h"
#include "util/logger.h"
#include "view/block/tab_item/spectrum_visualizer.h"
#include "view/block/tab_viewer.h"

namespace {

using ::testing::AllOf;
using ::testing::Field;
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
    screen = std::make_unique<ftxui::Screen>(84, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create TabViewer block
    block = ftxui::Make<interface::TabViewer>(dispatcher);
  }

  static constexpr int kNumberBars = 18;
};

/* ********************************************************************************************** */

TEST_F(TabViewerTest, InitialRender) {
  auto event_bars =
      interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(kNumberBars, 0.001));
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer ────────────────────────────────────────[F1:help]───[X]╮
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│     ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁ ▁▁▁      │
╰──────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, AnimationHorizontalMirror) {
  std::vector<double> values{0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.24, 0.15, 0.06,
                             0.81, 0.72, 0.61, 0.52, 0.41, 0.33, 0.24, 0.15, 0.06};

  auto event_bars = interface::CustomEvent::DrawAudioSpectrum(values);
  Process(event_bars);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer ────────────────────────────────────────[F1:help]───[X]╮
│                                                                                  │
│                                                                                  │
│                                     ▅▅▅ ▅▅▅                                      │
│                                 ▃▃▃ ███ ███ ▃▃▃                                  │
│                                 ███ ███ ███ ███                                  │
│                             ███ ███ ███ ███ ███ ███                              │
│                         ▇▇▇ ███ ███ ███ ███ ███ ███ ▇▇▇                          │
│                     ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ▃▃▃                      │
│                 ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▃▃▃                  │
│             ▁▁▁ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▁▁▁              │
│             ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███              │
│         ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███          │
│     ▇▇▇ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▇▇▇      │
╰──────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, AnimationVerticalMirror) {
  std::vector<double> values{
      0.1, 0.2, 0.3, 0.4, 0.5, 0.4, 0.3, 0.2, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95,
      0.1, 0.2, 0.3, 0.4, 0.5, 0.4, 0.3, 0.2, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95,
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
╭ 1:visualizer  2:equalizer ────────────────────────────────────────[F1:help]───[X]╮
│                                                                     ▄▄▄ ▆▆▆      │
│                                                             ▂▂▂ ▇▇▇ ███ ███      │
│                                                         ▅▅▅ ███ ███ ███ ███      │
│                 ▄▄▄ ███ ▄▄▄                     ▄▄▄ ███ ███ ███ ███ ███ ███      │
│         ▂▂▂ ▇▇▇ ███ ███ ███ ▇▇▇ ▂▂▂     ▂▂▂ ▇▇▇ ███ ███ ███ ███ ███ ███ ███      │
│     ▅▅▅ ███ ███ ███ ███ ███ ███ ███ ▅▅▅ ███ ███ ███ ███ ███ ███ ███ ███ ███      │
│     ▃▃▃                             ▃▃▃                                          │
│     ███ ▅▅▅                     ▅▅▅ ███ ▅▅▅                                      │
│     ███ ███ ███ ▂▂▂     ▂▂▂ ███ ███ ███ ███ ███ ▂▂▂                              │
│     ███ ███ ███ ███ ▄▄▄ ███ ███ ███ ███ ███ ███ ███ ▄▄▄                          │
│     ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▇▇▇ ▁▁▁                  │
│     ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▄▄▄              │
│     ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ▆▆▆ ▃▃▃      │
╰──────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, AnimationMono) {
  std::vector<double> values{
      0.1, 0.2, 0.3, 0.4, 0.5, 0.4, 0.3, 0.2, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95,
      0.1, 0.2, 0.3, 0.4, 0.5, 0.4, 0.3, 0.2, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.95,
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
╭ 1:visualizer  2:equalizer ────────────────────────────────────────[F1:help]───[X]╮
│                                                                         ▃▃▃      │
│                                                                     ▆▆▆ ███      │
│                                                                 ▄▄▄ ███ ███      │
│                                                             ▁▁▁ ███ ███ ███      │
│                                                             ███ ███ ███ ███      │
│                                                         ▇▇▇ ███ ███ ███ ███      │
│                     ▄▄▄                             ▄▄▄ ███ ███ ███ ███ ███      │
│                 ▂▂▂ ███ ▂▂▂                     ▂▂▂ ███ ███ ███ ███ ███ ███      │
│                 ███ ███ ███                     ███ ███ ███ ███ ███ ███ ███      │
│             ███ ███ ███ ███ ███             ███ ███ ███ ███ ███ ███ ███ ███      │
│         ▅▅▅ ███ ███ ███ ███ ███ ▅▅▅     ▅▅▅ ███ ███ ███ ███ ███ ███ ███ ███      │
│     ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ▃▃▃ ███ ███ ███ ███ ███ ███ ███ ███ ███      │
│     ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███ ███      │
╰──────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, RenderEqualizer) {
  auto event_bars =
      interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(kNumberBars, 0.001));
  Process(event_bars);

  block->OnEvent(ftxui::Event::Character('2'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer ────────────────────────────────────────[F1:help]───[X]╮
│                                                                                  │
│ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz    16 kHz  │
│                                                                                  │
│                                                                                  │
│                                                                                  │
│   ██      ██      ██      ██      ██      ██      ██      ██      ██       ██    │
│   ██      ██      ██      ██      ██      ██      ██      ██      ██       ██    │
│                                                                                  │
│   0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB    0 dB     0 dB  │
│                                                                                  │
│                          ┌─────────────┐┌─────────────┐                          │
│                          │    Apply    ││    Reset    │                          │
│                          └─────────────┘└─────────────┘                          │
╰──────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace
