
#include <gmock/gmock-matchers.h>   // for StrEq, EXPECT_THAT
#include <gtest/gtest-message.h>    // for Message
#include <gtest/gtest-test-part.h>  // for TestPartResult

#include <filesystem>  // for current_path, path
#include <memory>      // for __shared_ptr_access

#include "ftxui/component/component.hpp"       // for Make
#include "ftxui/component/component_base.hpp"  // for Component, ComponentBase
#include "ftxui/component/event.hpp"           // for Event, Event::ArrowDown
#include "ftxui/dom/node.hpp"                  // for Render
#include "ftxui/screen/screen.hpp"             // for Screen
#include "general/block.h"
#include "general/utils.h"          // for FilterAnsiCommands
#include "gtest/gtest_pred_impl.h"  // for SuiteApiResolver, TEST_F
#include "mock/event_dispatcher_mock.h"
#include "mock/list_directory_mock.h"
#include "util/logger.h"

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::StrEq;
using ::testing::VariantWith;

//! Create custom matcher to compare only filename from std::filesystem::path
MATCHER_P(IsSameFilename, n, "") { return arg.filename() == n; }

/**
 * @brief Tests with ListDirectory class
 */
class ListDirectoryTest : public ::BlockTest {
 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(32, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // use test directory as base dir
    std::string source_dir{LISTDIR_PATH};
    block = ftxui::Make<ListDirectoryMock>(dispatcher, source_dir);

    // Set this block as focused
    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);
  }
};

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, InitialRender) {
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│> ..                          │
│  audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│  util_argparser.cc           │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NavigateOnMenu) {
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Tab);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::TabReverse);
  block->OnEvent(ftxui::Event::ArrowDown);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│  ..                          │
│  audio_player.cc             │
│  block_file_info.cc          │
│> block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│  util_argparser.cc           │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NavigateToMockDir) {
  block->OnEvent(ftxui::Event::End);
  block->OnEvent(ftxui::Event::ArrowUp);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│mock                          │
│> ..                          │
│  analyzer_mock.h             │
│  audio_control_mock.h        │
│  decoder_mock.h              │
│  event_dispatcher_mock.h     │
│  interface_notifier_mock.h   │
│  list_directory_mock.h       │
│  playback_mock.h             │
│                              │
│                              │
│                              │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, EnterOnSearchMode) {
  block->OnEvent(ftxui::Event::Character('/'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│> ..                          │
│  audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│Search:                       │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, SingleCharacterInSearchMode) {
  std::string typed{"/e"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│> audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  util_argparser.cc           │
│                              │
│Search:e                      │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, TextAndNavigateInSearchMode) {
  std::string typed{"/mock"};
  utils::QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│mock                          │
│> ..                          │
│  analyzer_mock.h             │
│  audio_control_mock.h        │
│  decoder_mock.h              │
│  event_dispatcher_mock.h     │
│  interface_notifier_mock.h   │
│  list_directory_mock.h       │
│  playback_mock.h             │
│                              │
│                              │
│                              │
│                              │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NonExistentTextInSearchMode) {
  std::string typed{"/inexistentfilename"};
  utils::QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│Search:inexistentfilename     │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, EnterAndExitSearchMode) {
  block->OnEvent(ftxui::Event::Character('/'));
  block->OnEvent(ftxui::Event::Escape);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│> ..                          │
│  audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│  util_argparser.cc           │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NotifyFileSelection) {
  // Setup expectation for event sending
  std::filesystem::path file{"audio_player.cc"};
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(IsSameFilename(file))))))
      .Times(1);

  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│  ..                          │
│> audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│  util_argparser.cc           │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, RunTextAnimation) {
  // Hacky method to add new entry
  auto list_dir = std::static_pointer_cast<interface::ListDirectory>(block);
  std::filesystem::path dummy{"this_is_a_really_long_pathname.mp3"};
  list_dir->entries_.emplace_back(dummy);

  // Setup expectation for event sending (to refresh UI)
  // p.s.: Times(5) is based on refresh timing from thread animation
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::Refresh)))
      .Times(5);

  block->OnEvent(ftxui::Event::End);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│  audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│  util_argparser.cc           │
│> this_is_a_really_long_pathna│
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Wait for a few moments to render again and see that text has changed
  screen->Clear();

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(1.1s);

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ files ───────────────────────╮
│test                          │
│  audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│  util_argparser.cc           │
│> is_a_really_long_pathname.mp│
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, TryToNavigateOnEmptySearch) {
  std::string typed{"/notsomethingthatexists"};
  utils::QueueCharacterEvents(*block, typed);

  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│Search:notsomethingthatexists │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, NavigateAndEraseCharactersOnSearch) {
  std::string typed{"/block"};
  utils::QueueCharacterEvents(*block, typed);

  block->OnEvent(ftxui::Event::ArrowLeft);
  block->OnEvent(ftxui::Event::ArrowLeft);
  block->OnEvent(ftxui::Event::ArrowLeft);
  block->OnEvent(ftxui::Event::ArrowLeft);
  block->OnEvent(ftxui::Event::Backspace);

  block->OnEvent(ftxui::Event::ArrowRight);
  block->OnEvent(ftxui::Event::ArrowRight);
  block->OnEvent(ftxui::Event::Backspace);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│                              │
│Search:lck                    │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, ScrollMenuOnBigList) {
  // Hacky method to add new entries until it fills the screen
  auto list_dir = std::static_pointer_cast<interface::ListDirectory>(block);
  for (int i = 0; i < 5; i++) {
    std::filesystem::path dummy{"some_music_" + std::to_string(i) + ".mp3"};
    list_dir->entries_.emplace_back(dummy);
  }

  // Navigate to the end and check if list moves on the screen according to selected entry
  block->OnEvent(ftxui::Event::End);
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│  util_argparser.cc           │
│  some_music_0.mp3            │
│  some_music_1.mp3            │
│  some_music_2.mp3            │
│  some_music_3.mp3            │
│> some_music_4.mp3            │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, PlayNextFileAfterFinished) {
  InSequence seq;
  auto derived = std::static_pointer_cast<interface::ListDirectory>(block);

  // Setup expectation to play first file
  std::filesystem::path file{LISTDIR_PATH + std::string{"/audio_player.cc"}};
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(file)))))
      .Times(1);

  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│  ..                          │
│> audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│  util_argparser.cc           │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Simulate player sending event to update song info and check internal state
  auto event_update = interface::CustomEvent::UpdateSongInfo(model::Song{.filepath = file,
                                                                         .artist = "Dummy artist",
                                                                         .title = "Dummy title",
                                                                         .num_channels = 2,
                                                                         .sample_rate = 44100,
                                                                         .bit_rate = 320000,
                                                                         .bit_depth = 32,
                                                                         .duration = 120});

  derived->OnCustomEvent(event_update);
  EXPECT_EQ(file, derived->curr_playing_.value());

  // Simulate player sending event to notify that song has ended
  auto event_finish = interface::CustomEvent::UpdateSongState(
      model::Song::CurrentInformation{.state = model::Song::MediaState::Finished});

  std::filesystem::path next_file{LISTDIR_PATH + std::string{"/block_file_info.cc"}};

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(next_file)))))
      .Times(1);

  derived->OnCustomEvent(event_finish);

  // Simulate player sending event with new song update
  auto& content = std::get<model::Song>(event_update.content);
  content.filepath = next_file;

  derived->OnCustomEvent(event_update);
  EXPECT_EQ(next_file, derived->curr_playing_.value());
}

/* ********************************************************************************************** */

TEST_F(ListDirectoryTest, StartPlayingLastFileAndPlayNextAfterFinished) {
  InSequence seq;
  auto derived = std::static_pointer_cast<interface::ListDirectory>(block);

  // Setup expectation to play last file
  std::filesystem::path file{LISTDIR_PATH + std::string{"/util_argparser.cc"}};
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(file)))))
      .Times(1);

  block->OnEvent(ftxui::Event::End);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ files ───────────────────────╮
│test                          │
│  ..                          │
│  audio_player.cc             │
│  block_file_info.cc          │
│  block_list_directory.cc     │
│  block_media_player.cc       │
│  block_tab_viewer.cc         │
│  CMakeLists.txt              │
│  driver_fftw.cc              │
│  general                     │
│  middleware_media_controller.│
│  mock                        │
│> util_argparser.cc           │
╰──────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Simulate player sending event to update song info and check internal state
  auto event_update = interface::CustomEvent::UpdateSongInfo(model::Song{.filepath = file,
                                                                         .artist = "Dummy artist",
                                                                         .title = "Dummy title",
                                                                         .num_channels = 2,
                                                                         .sample_rate = 44100,
                                                                         .bit_rate = 320000,
                                                                         .bit_depth = 32,
                                                                         .duration = 120});

  derived->OnCustomEvent(event_update);
  EXPECT_EQ(file, derived->curr_playing_.value());

  // Simulate player sending event to notify that song has ended
  auto event_finish = interface::CustomEvent::UpdateSongState(
      model::Song::CurrentInformation{.state = model::Song::MediaState::Finished});

  std::filesystem::path next_file{LISTDIR_PATH + std::string{"/audio_player.cc"}};

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(next_file)))))
      .Times(1);

  derived->OnCustomEvent(event_finish);

  // Simulate player sending event with new song update
  auto& content = std::get<model::Song>(event_update.content);
  content.filepath = next_file;

  derived->OnCustomEvent(event_update);
  EXPECT_EQ(next_file, derived->curr_playing_.value());
}

/* ********************************************************************************************** */

/**
 * @brief Tests with original ListDirectory class
 */
class ListDirectoryCtorTest : public ::BlockTest {
 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(32, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();
  }
};

TEST_F(ListDirectoryCtorTest, CreateWithBadInitialPath) {
  // Setup expectation
  EXPECT_CALL(*dispatcher, SetApplicationError(Eq(error::kAccessDirFailed)));

  // Use bad path as base dir, block will notify an error about not being to access it
  std::string source_dir{"/path/that/does/not/exist"};
  block = ftxui::Make<interface::ListDirectory>(dispatcher, source_dir);

  // After this error, block should use current path to list files
  auto list_dir = std::static_pointer_cast<interface::ListDirectory>(block);
  EXPECT_EQ(list_dir->curr_dir_, std::filesystem::current_path());
}

}  // namespace
