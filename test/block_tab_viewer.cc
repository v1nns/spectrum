#include <gmock/gmock-matchers.h>  // for StrEq, EXPECT_THAT

#include "general/block.h"
#include "general/utils.h"  // for FilterAnsiCommands
#include "mock/event_dispatcher_mock.h"
#include "util/logger.h"
#include "view/block/tab_item/spectrum_visualizer.h"
#include "view/block/tab_viewer.h"

namespace {

using ::testing::_;
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

    // Set this block as focused
    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);
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

/* ********************************************************************************************** */

TEST_F(TabViewerTest, ModifyEqualizerAndApply) {
  // Setup expectation for event to set focus on this tab view
  EXPECT_CALL(
      *dispatcher,
      SendEvent(
          AllOf(Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::SetFocused),
                Field(&interface::CustomEvent::content,
                      VariantWith<model::BlockIdentifier>(model::BlockIdentifier::TabViewer)))));

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
  std::vector<AudioFilter> audio_filters{
      AudioFilter{.frequency = 32},   AudioFilter{.frequency = 64, .gain = 5},
      AudioFilter{.frequency = 125},  AudioFilter{.frequency = 250, .gain = -2},
      AudioFilter{.frequency = 500},  AudioFilter{.frequency = 1000, .gain = -3},
      AudioFilter{.frequency = 2000}, AudioFilter{.frequency = 4000, .gain = 7},
      AudioFilter{.frequency = 8000}, AudioFilter{.frequency = 16000},
  };

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ApplyAudioFilters),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::vector<AudioFilter>>(audio_filters)))));

  // Apply EQ
  block->OnEvent(ftxui::Event::Character('a'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer ────────────────────────────────────────[F1:help]───[X]╮
│                                                                                  │
│ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz    16 kHz  │
│                                                                                  │
│                                                           ▂▂                     │
│           ▇▇                                              ██                     │
│   ██      ██      ██      ▆▆      ██      ▄▄      ██      ██      ██       ██    │
│   ██      ██      ██      ██      ██      ██      ██      ██      ██       ██    │
│                                                                                  │
│   0 dB    5 dB    0 dB   -2 dB    0 dB   -3 dB    0 dB    7 dB    0 dB     0 dB  │
│                                                                                  │
│                          ┌─────────────┐┌─────────────┐                          │
│                          │    Apply    ││    Reset    │                          │
│                          └─────────────┘└─────────────┘                          │
╰──────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(TabViewerTest, ModifyEqualizerAndReset) {
  // Setup expectation for event to set focus on this tab view
  EXPECT_CALL(
      *dispatcher,
      SendEvent(
          AllOf(Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::SetFocused),
                Field(&interface::CustomEvent::content,
                      VariantWith<model::BlockIdentifier>(model::BlockIdentifier::TabViewer)))));

  block->OnEvent(ftxui::Event::Character('2'));

  // Change 250Hz frequency (using keybindings for frequency navigation)
  std::string typed{"llllkkkkk"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ 1:visualizer  2:equalizer ────────────────────────────────────────[F1:help]───[X]╮
│                                                                                  │
│ 32 Hz   64 Hz   125 Hz  250 Hz  500 Hz  1 kHz   2 kHz   4 kHz   8 kHz    16 kHz  │
│                                                                                  │
│                                                                                  │
│                           ▇▇                                                     │
│   ██      ██      ██      ██      ██      ██      ██      ██      ██       ██    │
│   ██      ██      ██      ██      ██      ██      ██      ██      ██       ██    │
│                                                                                  │
│   0 dB    0 dB    0 dB    5 dB    0 dB    0 dB    0 dB    0 dB    0 dB     0 dB  │
│                                                                                  │
│                          ┌─────────────┐┌─────────────┐                          │
│                          │    Apply    ││    Reset    │                          │
│                          └─────────────┘└─────────────┘                          │
╰──────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation to check that will not send any audio filters
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ApplyAudioFilters),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::vector<model::AudioFilter>>(_)))))
      .Times(0);

  // Reset EQ
  block->OnEvent(ftxui::Event::Character('r'));

  // And try to apply EQ
  block->OnEvent(ftxui::Event::Character('a'));

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
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
