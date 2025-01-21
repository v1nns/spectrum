#include <gmock/gmock-matchers.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <filesystem>
#include <memory>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include "general/block.h"
#include "general/utils.h"
#include "gmock/gmock.h"
#include "mock/event_dispatcher_mock.h"
#include "mock/file_handler_mock.h"
#include "view/block/sidebar.h"
#include "view/block/sidebar_content/list_directory.h"
#include "view/block/sidebar_content/playlist_viewer.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrEq;
using ::testing::VariantWith;

//! Create custom matcher to compare only filename from std::filesystem::path
MATCHER_P(IsSameFilename, n, "") { return arg.filename() == n; }

/**
 * @brief Tests with Sidebar class
 */
class SidebarTest : public ::BlockTest {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(38, 15);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create mock for file handler
    EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_)).WillOnce(Return(true));

    // Use test directory as base dir
    block = ftxui::Make<interface::Sidebar>(dispatcher, LISTDIR_PATH, file_handler_mock_);

    // Set this block as focused
    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);
  }

  /* ******************************************************************************************** */
  //! ListDirectory

  //! Getter for ListDirectory (necessary as inner variable is an unique_ptr)
  auto GetListDirectory() -> interface::ListDirectory* {
    auto sidebar = std::static_pointer_cast<interface::Sidebar>(block);
    return reinterpret_cast<interface::ListDirectory*>(
        sidebar->tab_elem_[interface::Sidebar::View::Files].get());
  }

  //! Getter for current playing file from ListDirectory
  auto GetCurrentPlaying() -> std::filesystem::path {
    return GetListDirectory()->curr_playing_.value();
  }

  //! Getter for current dir from ListDirectory
  auto GetCurrentDir() -> std::filesystem::path { return GetListDirectory()->GetCurrentDir(); }

  //! Hacky method to add new entry in files tab_item
  void EmplaceFile(const std::filesystem::path& entry) {
    auto files = GetListDirectory();
    files->menu_->Emplace(entry);
  }

  /* ******************************************************************************************** */
  //! PlaylistViewer

  //! Getter for PlaylistViewer (necessary as inner variable is an unique_ptr)
  auto GetPlaylistViewer() -> interface::PlaylistViewer* {
    auto sidebar = std::static_pointer_cast<interface::Sidebar>(block);
    return reinterpret_cast<interface::PlaylistViewer*>(
        sidebar->tab_elem_[interface::Sidebar::View::Playlist].get());
  }

  //! Getter for Modify button state
  bool IsModifyButtonActive() { return GetPlaylistViewer()->btn_modify_->IsActive(); }

  //! Getter for Delete button state
  bool IsDeleteButtonActive() { return GetPlaylistViewer()->btn_delete_->IsActive(); }

  //!< Mock for file handler
  std::shared_ptr<FileHandlerMock> file_handler_mock_ = std::make_shared<FileHandlerMock>();
};

/* ********************************************************************************************** */

/**
 * @brief Tests with original ListDirectory class
 */
class ListDirectoryCtorTest : public ::SidebarTest {
 protected:
  void SetUp() override {
    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();
  }
};

TEST_F(ListDirectoryCtorTest, CreateWithBadInitialPath) {
  // Setup expectation
  EXPECT_CALL(*dispatcher, SetApplicationError(Eq(error::kAccessDirFailed))).Times(0);

  // Use bad path as base dir, block will notify an error about not being to access it
  std::string source_dir{"/path/that/does/not/exist"};
  block = ftxui::Make<interface::Sidebar>(dispatcher, source_dir);

  // After this error, block should use current path to list files
  EXPECT_EQ(GetCurrentDir(), std::filesystem::current_path());
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, InitialRender) {
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│▶ ..                                │
│  audio_lyric_finder.cc             │
│  audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NavigateOnMenu) {
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Tab);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::TabReverse);
  block->OnEvent(ftxui::Event::ArrowDown);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│  ..                                │
│  audio_lyric_finder.cc             │
│  audio_player.cc                   │
│▶ block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NavigateToMockDir) {
  block->OnEvent(ftxui::Event::End);
  block->OnEvent(ftxui::Event::ArrowUp);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│mock                                │
│▶ ..                                │
│  analyzer_mock.h                   │
│  audio_control_mock.h              │
│  decoder_mock.h                    │
│  event_dispatcher_mock.h           │
│  file_handler_mock.h               │
│  html_parser_mock.h                │
│  interface_notifier_mock.h         │
│  lyric_finder_mock.h               │
│  playback_mock.h                   │
│  url_fetcher_mock.h                │
│                                    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, EnterOnSearchMode) {
  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  block->OnEvent(ftxui::Event::Character('/'));

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│▶ ..                                │
│  audio_lyric_finder.cc             │
│  audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│Search:                             │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, SingleCharacterInSearchMode) {
  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  std::string typed{"/e"};
  utils::QueueCharacterEvents(*block, typed);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│▶ audio_lyric_finder.cc             │
│  audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
│  util_argparser.cc                 │
│Search:e                            │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, TextAndNavigateInSearchMode) {
  // Setup expectation for event disabling/enabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);

  std::string typed{"/mock"};
  utils::QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│mock                                │
│▶ ..                                │
│  analyzer_mock.h                   │
│  audio_control_mock.h              │
│  decoder_mock.h                    │
│  event_dispatcher_mock.h           │
│  file_handler_mock.h               │
│  html_parser_mock.h                │
│  interface_notifier_mock.h         │
│  lyric_finder_mock.h               │
│  playback_mock.h                   │
│  url_fetcher_mock.h                │
│                                    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NonExistentTextInSearchMode) {
  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  std::string typed{"/inexistentfilename"};
  utils::QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│Search:inexistentfilename           │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, EnterAndExitSearchMode) {
  // Setup expectation for event disabling/enabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);

  block->OnEvent(ftxui::Event::Character('/'));
  block->OnEvent(ftxui::Event::Escape);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│▶ ..                                │
│  audio_lyric_finder.cc             │
│  audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, EnterSearchModeTypeKeybindAndExit) {
  // Setup expectation for event disabling/enabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);

  std::string typed{"/q"};
  utils::QueueCharacterEvents(*block, typed);
  block->OnEvent(ftxui::Event::Escape);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│▶ ..                                │
│  audio_lyric_finder.cc             │
│  audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, EnterSearchModeAndNotifyFileSelection) {
  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  // Setup expectation for file selection
  std::filesystem::path file{LISTDIR_PATH + std::string("/audio_player.cc")};
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(file)))))
      .WillOnce(Invoke([&](const interface::CustomEvent&) {
        // As we don't have an instance of Terminal, process custom event directly
        auto derived = GetListDirectory();

        // Send event simulating the audio thread notifying that is playing a new song
        auto update_song = interface::CustomEvent::UpdateSongInfo(model::Song{.filepath = file});
        derived->OnCustomEvent(update_song);
      }));

  std::string typed{"/player"};
  utils::QueueCharacterEvents(*block, typed);

  // Setup expectation for event enabling global mode again
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│  ..                                │
│  audio_lyric_finder.cc             │
│▶ audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  typed = "/..";
  utils::QueueCharacterEvents(*block, typed);

  // Setup expectation for event enabling global mode again
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);
  block->OnEvent(ftxui::Event::Return);

  // Clear screen and check for new render state
  screen->Clear();

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│spectrum                            │
│▶ ..                                │)";

  // Instead of checking for the whole list, just check that changed the base directory
  EXPECT_THAT(rendered, HasSubstr(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NotifyFileSelection) {
  // Setup expectation for event sending
  std::filesystem::path file{"audio_player.cc"};
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(IsSameFilename(file))))))
      .Times(1);

  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│  ..                                │
│  audio_lyric_finder.cc             │
│▶ audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, RunTextAnimation) {
  // Hacky method to add new entry
  EmplaceFile(std::filesystem::path{"this_is_a_really_long_pathname_to_test.mp3"});

  // Setup expectation for event sending (to refresh UI)
  // p.s.: Times(5) is based on refresh timing from thread animation
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::Refresh)))
      .Times(5);

  block->OnEvent(ftxui::Event::End);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
│  mock                              │
│  util_argparser.cc                 │
│▶ this_is_a_really_long_pathname_to_│
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Wait for a few moments to render again and see that text has changed
  screen->Clear();

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(1.1s);

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
│  mock                              │
│  util_argparser.cc                 │
│▶ is_a_really_long_pathname_to_test.│
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, TryToNavigateOnEmptySearch) {
  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  std::string typed{"/notsomethingthatexists"};
  utils::QueueCharacterEvents(*block, typed);

  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│Search:notsomethingthatexists       │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NavigateAndEraseCharactersOnSearch) {
  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

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
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│Search:lck                          │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, ScrollMenuOnBigList) {
  // Hacky method to add new entries until it fills the screen
  for (int i = 0; i < 5; i++) {
    EmplaceFile(std::filesystem::path{"some_music_" + std::to_string(i) + ".mp3"});
  }

  // Navigate to the end and check if list moves on the screen according to selected entry
  block->OnEvent(ftxui::Event::End);
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
│  mock                              │
│  util_argparser.cc                 │
│  some_music_0.mp3                  │
│  some_music_1.mp3                  │
│  some_music_2.mp3                  │
│  some_music_3.mp3                  │
│▶ some_music_4.mp3                  │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, PlayNextFileAfterFinished) {
  InSequence seq;
  auto derived = GetListDirectory();

  // Setup expectation to play first file
  std::filesystem::path file{LISTDIR_PATH + std::string{"/audio_player.cc"}};
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(file)))))
      .Times(1);

  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│  ..                                │
│  audio_lyric_finder.cc             │
│▶ audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
╰────────────────────────────────────╯)";

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
  EXPECT_EQ(file, GetCurrentPlaying());

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
  EXPECT_EQ(next_file, GetCurrentPlaying());
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, StartPlayingLastFileAndPlayNextAfterFinished) {
  InSequence seq;
  auto derived = GetListDirectory();

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
╭ F1:files  F2:playlist ─────────────╮
│test                                │
│  audio_player.cc                   │
│  block_file_info.cc                │
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  general                           │
│  middleware_media_controller.cc    │
│  mock                              │
│▶ util_argparser.cc                 │
╰────────────────────────────────────╯)";

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
  EXPECT_EQ(file, GetCurrentPlaying());

  // Simulate player sending event to notify that song has ended
  auto event_finish = interface::CustomEvent::UpdateSongState(
      model::Song::CurrentInformation{.state = model::Song::MediaState::Finished});

  std::filesystem::path next_file{LISTDIR_PATH + std::string{"/audio_lyric_finder.cc"}};

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
  EXPECT_EQ(next_file, GetCurrentPlaying());
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, EmptyPlaylist) {
  model::Playlists data{};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, SinglePlaylist) {
  model::Playlists data{{
      model::Playlist{
          .index = 0,
          .name = "Chill mix",
          .songs = {model::Song{.filepath = "chilling 1.mp3"},
                    model::Song{.filepath = "chilling 2.mp3"},
                    model::Song{.filepath = "chilling 2.mp3"}},
      },
  }};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ Chill mix [3]                     │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NavigateOnPlaylist) {
  model::Playlists data{{
      model::Playlist{
          .index = 0,
          .name = "Chill mix",
          .songs =
              {
                  model::Song{.filepath = "chilling 1.mp3"},
                  model::Song{.filepath = "chilling 2.mp3"},
                  model::Song{.filepath = "chilling 3.mp3"},
              },
      },
      model::Playlist{
          .index = 1,
          .name = "Lofi",
          .songs =
              {
                  model::Song{.filepath = "lofi 1.mp3"},
                  model::Song{.filepath = "lofi 2.mp3"},
                  model::Song{.filepath = "lofi 2.mp3"},
              },
      },
  }};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ Chill mix [3]                     │
│  Lofi [3]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Open first playlist and select last song
  std::string typed{"ljjj"};
  utils::QueueCharacterEvents(*block, typed);

  // Clear screen and check for new render state
  screen->Clear();

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix [3]                     │
│    chilling 1.mp3                  │
│    chilling 2.mp3                  │
│▶   chilling 3.mp3                  │
│  Lofi [3]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Now close first playlist, open the second one and select second song
  block->OnEvent(ftxui::Event::Home);

  typed = "hjljj";
  utils::QueueCharacterEvents(*block, typed);

  // Clear screen and check for new render state
  screen->Clear();

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix [3]                     │
│  Lofi [3]                          │
│    lofi 1.mp3                      │
│▶   lofi 2.mp3                      │
│    lofi 2.mp3                      │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, SearchOnPlaylistAndNotify) {
  model::Playlists data{{
      model::Playlist{
          .index = 0,
          .name = "Chill mix",
          .songs =
              {
                  model::Song{.filepath = "chilling 1.mp3"},
                  model::Song{.filepath = "chilling 2.mp3"},
                  model::Song{.filepath = "chilling 3.mp3"},
              },
      },
      model::Playlist{
          .index = 1,
          .name = "Lofi",
          .songs =
              {
                  model::Song{.filepath = "lofi 1.mp3"},
                  model::Song{.filepath = "lofi 2.mp3"},
                  model::Song{.filepath = "lofi 3.mp3"},
              },
      },
  }};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  // Enable search and look for a lofi song
  std::string typed{"/lofi 2"};
  utils::QueueCharacterEvents(*block, typed);

  // Select song itself
  block->OnEvent(ftxui::Event::ArrowDown);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Lofi [1]                          │
│▶   lofi 2.mp3                      │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│Search:lofi 2                       │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation for event enabling global mode again
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);

  // Setup expectation for playlist sent by element (should be shuffled based on selected entry)
  model::Playlist playlist{
      .index = 1,
      .name = "Lofi",
      .songs =
          {
              model::Song{.filepath = "lofi 2.mp3"},
              model::Song{.filepath = "lofi 3.mp3"},
              model::Song{.filepath = "lofi 1.mp3"},
          },
  };

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyPlaylistSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::Playlist>(playlist)))));

  // Execute action on selected entry
  block->OnEvent(ftxui::Event::Return);
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NotifyLastPlaylist) {
  model::Playlists data{{
      model::Playlist{
          .index = 0,
          .name = "Chill mix",
          .songs =
              {
                  model::Song{.filepath = "chilling 1.mp3"},
                  model::Song{.filepath = "chilling 2.mp3"},
                  model::Song{.filepath = "chilling 3.mp3"},
              },
      },
      model::Playlist{
          .index = 1,
          .name = "Lofi",
          .songs =
              {
                  model::Song{.filepath = "lofi 1.mp3"},
                  model::Song{.filepath = "lofi 2.mp3"},
                  model::Song{.filepath = "lofi 3.mp3"},
              },
      },
      model::Playlist{
          .index = 2,
          .name = "Electro",
          .songs =
              {
                  model::Song{.filepath = "electro 1.mp3"},
                  model::Song{.filepath = "electro 2.mp3"},
              },
      },
  }};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Select last playlist and play
  std::string typed{"jj"};
  utils::QueueCharacterEvents(*block, typed);

  // Setup expectation for playlist sent by element
  model::Playlist playlist{
      .index = 2,
      .name = "Electro",
      .songs =
          {
              model::Song{.filepath = "electro 1.mp3"},
              model::Song{.filepath = "electro 2.mp3"},
          },
  };

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyPlaylistSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::Playlist>(playlist)))));

  // Execute action on selected entry
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix [3]                     │
│  Lofi [3]                          │
│▶ Electro [2]                       │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, RunTextAnimationOnPlaylistName) {
  model::Playlists data{
      {model::Playlist{
           .index = 0,
           .name = "Chill mix really long and the coolest of them all",
           .songs =
               {
                   model::Song{.filepath = "chilling 1.mp3"},
                   model::Song{.filepath = "chilling 3.mp3"},
                   model::Song{.filepath = "chilling with a really long name.mp3"},
               },
       },
       model::Playlist{
           .index = 1,
           .name = "Lofi",
           .songs =
               {
                   model::Song{.filepath = "lofi 1.mp3"},
                   model::Song{.filepath = "lofi 2.mp3"},
                   model::Song{.filepath = "lofi 3.mp3"},
               },
       }}};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Setup expectation for event sending (to refresh UI)
  // p.s.: Times(5) is based on refresh timing from thread animation
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::Refresh)))
      .Times(5);

  // Select last song from the first playlist
  std::string typed{"l"};
  utils::QueueCharacterEvents(*block, typed);

  // Render element
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ Chill mix really long and the cool│
│    chilling 1.mp3                  │
│    chilling 3.mp3                  │
│    chilling with a really long name│
│  Lofi [3]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Wait for a few moments to render again and see that text has changed
  screen->Clear();

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(1.1s);

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶  mix really long and the coolest o│
│    chilling 1.mp3                  │
│    chilling 3.mp3                  │
│    chilling with a really long name│
│  Lofi [3]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Select first song and check that animation will stop
  typed = "j";
  utils::QueueCharacterEvents(*block, typed);

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix really long and the cool│
│▶   chilling 1.mp3                  │
│    chilling 3.mp3                  │
│    chilling with a really long name│
│  Lofi [3]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, RunTextAnimationOnPlaylistSong) {
  model::Playlists data{
      {model::Playlist{
           .index = 0,
           .name = "Chill mix",
           .songs =
               {
                   model::Song{.filepath = "chilling 1.mp3"},
                   model::Song{.filepath = "chilling 3.mp3"},
                   model::Song{.filepath = "chilling with a really long name.mp3"},
               },
       },
       model::Playlist{
           .index = 1,
           .name = "Lofi",
           .songs =
               {
                   model::Song{.filepath = "lofi 1.mp3"},
                   model::Song{.filepath = "lofi 2.mp3"},
                   model::Song{.filepath = "lofi 3.mp3"},
               },
       }}};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Setup expectation for event sending (to refresh UI)
  // p.s.: Times(5) is based on refresh timing from thread animation
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::Refresh)))
      .Times(5);

  // Select last song from the first playlist
  std::string typed{"ljjj"};
  utils::QueueCharacterEvents(*block, typed);

  // Render element
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix [3]                     │
│    chilling 1.mp3                  │
│    chilling 3.mp3                  │
│▶   chilling with a really long name│
│  Lofi [3]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Wait for a few moments to render again and see that text has changed
  screen->Clear();

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(1.1s);

  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix [3]                     │
│    chilling 1.mp3                  │
│    chilling 3.mp3                  │
│▶   ing with a really long name.mp3 │
│  Lofi [3]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Open next playlist and check that animation will stop
  typed = "jl";
  utils::QueueCharacterEvents(*block, typed);

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix [3]                     │
│    chilling 1.mp3                  │
│    chilling 3.mp3                  │
│    chilling with a really long name│
│▶ Lofi [3]                          │
│    lofi 1.mp3                      │
│    lofi 2.mp3                      │
│    lofi 3.mp3                      │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, ForceClickOnEmptyPlaylistWhileOnSearchMode) {
  model::Playlists data{{model::Playlist{
                             .index = 0,
                             .name = "Chill mix",
                             .songs =
                                 {
                                     model::Song{.filepath = "chilling 1.mp3"},
                                     model::Song{.filepath = "chilling 2.mp3"},
                                     model::Song{.filepath = "chilling 3.mp3"},
                                 },
                         },
                         model::Playlist{
                             .index = 1,
                             .name = "Lofi",
                             .songs = {},
                         }}};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  // Enter search mode and type some stuff
  std::string typed{"/lofi"};
  utils::QueueCharacterEvents(*block, typed);

  // Setup expectation for event enabling global mode again
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);

  // Must not send a playlist notification as it will be empty (no songs at all)
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::NotifyPlaylistSelection)))
      .Times(0);

  // Execute action on selected entry
  block->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ Chill mix [3]                     │
│  Lofi [0]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, ShowPlaylistManagerWithKeybindings) {
  model::Playlists data{{model::Playlist{
                             .index = 0,
                             .name = "Chill mix",
                             .songs =
                                 {
                                     model::Song{.filepath = "chilling 1.mp3"},
                                     model::Song{.filepath = "chilling 2.mp3"},
                                     model::Song{.filepath = "chilling 3.mp3"},
                                 },
                         },
                         model::Playlist{
                             .index = 1,
                             .name = "Lofi",
                             .songs = {},
                         }}};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Setup expectation for playlist operation
  model::PlaylistOperation expected_operation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  };

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ShowPlaylistManager),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::PlaylistOperation>(expected_operation)))));

  // Type keybind to show dialog for playlist creation
  std::string typed{"c"};
  utils::QueueCharacterEvents(*block, typed);

  expected_operation = {
      .action = model::PlaylistOperation::Operation::Modify,
      .playlist = data[0],
  };

  // Setup expectation for playlist operation
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ShowPlaylistManager),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::PlaylistOperation>(expected_operation)))));

  // Type keybind to show dialog for playlist modification
  typed = "o";
  utils::QueueCharacterEvents(*block, typed);

  model::QuestionData expected_question{
      .question = std::string("Do you want to delete \"" + data[0].name + "\"?"),
  };

  // Setup expectation for playlist operation
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ShowQuestionDialog),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::QuestionData>(expected_question)))))
      .WillOnce(Invoke([](const interface::CustomEvent& event) {
        // Check that one of these callbacks (yes) are not null and callable
        const auto& question_content = event.GetContent<model::QuestionData>();
        EXPECT_TRUE(question_content.cb_yes);
        EXPECT_FALSE(question_content.cb_no);
      }));

  // Type keybind to show dialog for playlist modification
  typed = "d";
  utils::QueueCharacterEvents(*block, typed);
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, SaveNewPlaylistIntoFile) {
  model::Playlists data{};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  model::Playlist playlist{.index = -1,
                           .name = "JPop",
                           .songs = {
                               model::Song{.filepath = "some of the coolest jpop 1.mp3"},
                               model::Song{.filepath = "some of the coolest jpop 2.mp3"},
                               model::Song{.filepath = "some of the coolest jpop 3.mp3"},
                           }};

  interface::CustomEvent save_playlist = interface::CustomEvent::SavePlaylistsToFile(playlist);

  // Setup expectation for saving playlist
  playlist.index = 0;  // index will get fixed at this point
  model::Playlists expected_playlists{playlist};

  EXPECT_CALL(*file_handler_mock_, SavePlaylists(expected_playlists)).WillOnce(Return(true));

  // Process custom event directly (in real life, it would be the playlist dialog sending it)
  GetPlaylistViewer()->OnCustomEvent(save_playlist);

  // Open new playlist
  block->OnEvent(ftxui::Event::Character('l'));

  // Check for rendered screen
  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ JPop [3]                          │
│    some of the coolest jpop 1.mp3  │
│    some of the coolest jpop 2.mp3  │
│    some of the coolest jpop 3.mp3  │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, SaveExistentPlaylistIntoFile) {
  model::Playlists data{{model::Playlist{
                             .index = 0,
                             .name = "Chill mix",
                             .songs =
                                 {
                                     model::Song{.filepath = "chilling 1.mp3"},
                                     model::Song{.filepath = "chilling 2.mp3"},
                                     model::Song{.filepath = "chilling 3.mp3"},
                                 },
                         },
                         {.index = 1,
                          .name = "JPop",
                          .songs =
                              {
                                  model::Song{.filepath = "some of the coolest jpop 1.mp3"},
                                  model::Song{.filepath = "some of the coolest jpop 2.mp3"},
                                  model::Song{.filepath = "some of the coolest jpop 3.mp3"},
                              }},
                         model::Playlist{
                             .index = 2,
                             .name = "Lofi",
                             .songs = {},
                         }}};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Get JPop playlist, remove second song and add a new one
  model::Playlist playlist = data[1];
  playlist.songs.erase(std::next(playlist.songs.begin()));
  playlist.songs.emplace_back(model::Song{.filepath = "some of the coolest jpop 55.mp3"});

  interface::CustomEvent save_playlist = interface::CustomEvent::SavePlaylistsToFile(playlist);

  // Setup expectation for saving playlist
  model::Playlists expected_playlists{data[0], playlist, data[2]};
  EXPECT_CALL(*file_handler_mock_, SavePlaylists(expected_playlists)).WillOnce(Return(true));

  // Process custom event directly (in real life, it would be the playlist dialog sending it)
  GetPlaylistViewer()->OnCustomEvent(save_playlist);

  // Open all playlists
  std::string typed{"ljjjjljjjjl"};
  utils::QueueCharacterEvents(*block, typed);

  // Check for rendered screen
  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix [3]                     │
│    chilling 1.mp3                  │
│    chilling 2.mp3                  │
│    chilling 3.mp3                  │
│  JPop [3]                          │
│    some of the coolest jpop 1.mp3  │
│    some of the coolest jpop 3.mp3  │
│    some of the coolest jpop 55.mp3 │
│▶ Lofi [0]                          │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, DeleteExistentPlaylist) {
  model::Playlists data{{model::Playlist{
                             .index = 0,
                             .name = "Chill mix",
                             .songs =
                                 {
                                     model::Song{.filepath = "chilling 1.mp3"},
                                     model::Song{.filepath = "chilling 2.mp3"},
                                     model::Song{.filepath = "chilling 3.mp3"},
                                 },
                         },
                         {.index = 1,
                          .name = "JPop",
                          .songs =
                              {
                                  model::Song{.filepath = "some of the coolest jpop 1.mp3"},
                                  model::Song{.filepath = "some of the coolest jpop 2.mp3"},
                                  model::Song{.filepath = "some of the coolest jpop 3.mp3"},
                              }},
                         model::Playlist{
                             .index = 2,
                             .name = "Lofi",
                             .songs = {},
                         }}};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  model::QuestionData expected_question{
      .question = std::string("Do you want to delete \"" + data[0].name + "\"?"),
  };

  // Setup expectation for playlist operation
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ShowQuestionDialog),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::QuestionData>(expected_question)))))
      .WillOnce(Invoke([&](const interface::CustomEvent& event) {
        // Check that one of these callbacks (yes) are not null and callable
        const auto& question_content = event.GetContent<model::QuestionData>();
        EXPECT_TRUE(question_content.cb_yes);
        EXPECT_FALSE(question_content.cb_no);

        // Setup expectation for playlists to be saved on file
        auto playlists_to_save = data;
        playlists_to_save.erase(playlists_to_save.begin());

        EXPECT_CALL(*file_handler_mock_, SavePlaylists(playlists_to_save)).WillOnce(Return(true));
        question_content.cb_yes();

        // Check for rendered screen
        ftxui::Render(*screen, block->Render());
        std::string rendered = utils::FilterAnsiCommands(screen->ToString());

        std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ JPop [3]                          │
│  Lofi [0]                          │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

        EXPECT_THAT(rendered, StrEq(expected));
      }));

  // Type keybind to show dialog for playlist modification
  block->OnEvent(ftxui::Event::Character('d'));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, StartEmptyAddNewPlaylistAndCheckButtonState) {
  model::Playlists data{};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Check for rendered screen
  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Check for buttons state
  EXPECT_FALSE(IsModifyButtonActive());
  EXPECT_FALSE(IsDeleteButtonActive());

  // Save a new playlist
  model::Playlist playlist{.index = 0,
                           .name = "Coding session",
                           .songs = {
                               model::Song{.filepath = "chilling 1.mp3"},
                               model::Song{.filepath = "chilling 2.mp3"},
                           }};

  interface::CustomEvent save_playlist = interface::CustomEvent::SavePlaylistsToFile(playlist);

  // Setup expectation for saving playlist
  model::Playlists expected_playlists{playlist};
  EXPECT_CALL(*file_handler_mock_, SavePlaylists(expected_playlists)).WillOnce(Return(true));

  // Process custom event directly (in real life, it would be the playlist dialog sending it)
  GetPlaylistViewer()->OnCustomEvent(save_playlist);

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ Coding session [2]                │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Check for buttons state
  EXPECT_TRUE(IsModifyButtonActive());
  EXPECT_TRUE(IsDeleteButtonActive());

  model::QuestionData expected_question{
      .question = std::string("Do you want to delete \"" + playlist.name + "\"?"),
  };

  // Setup expectation for playlist operation
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::ShowQuestionDialog),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::QuestionData>(expected_question)))))
      .WillOnce(Invoke([&](const interface::CustomEvent& event) {
        // Check that one of these callbacks (yes) are not null and callable
        const auto& question_content = event.GetContent<model::QuestionData>();
        EXPECT_TRUE(question_content.cb_yes);
        EXPECT_FALSE(question_content.cb_no);

        // Setup expectation for playlists to be saved on file
        EXPECT_CALL(*file_handler_mock_, SavePlaylists(model::Playlists{})).WillOnce(Return(true));
        question_content.cb_yes();
      }));

  // Type keybind to show dialog for playlist modification
  block->OnEvent(ftxui::Event::Character('d'));

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Check for buttons state
  EXPECT_FALSE(IsModifyButtonActive());
  EXPECT_FALSE(IsDeleteButtonActive());
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, CheckForToggleSupport) {
  model::Playlists data{{model::Playlist{.index = 0,
                                         .name = "Chill mix",
                                         .songs = {
                                             model::Song{.filepath = "chilling 1.mp3"},
                                             model::Song{.filepath = "chilling 2.mp3"},
                                             model::Song{.filepath = "chilling 3.mp3"},
                                         }}}};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);

  // Open all playlists
  std::string typed{"ll"};
  utils::QueueCharacterEvents(*block, typed);

  // Check for rendered screen
  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ Chill mix [3]                     │
│    chilling 1.mp3                  │
│    chilling 2.mp3                  │
│    chilling 3.mp3                  │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  typed = "hh";
  utils::QueueCharacterEvents(*block, typed);

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ Chill mix [3]                     │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Toggle playlist
  block->OnEvent(ftxui::Event::Character(' '));

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│▶ Chill mix [3]                     │
│    chilling 1.mp3                  │
│    chilling 2.mp3                  │
│    chilling 3.mp3                  │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Nothing should happen on song entry
  typed = "jlh";
  utils::QueueCharacterEvents(*block, typed);

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ F1:files  F2:playlist ─────────────╮
│  Chill mix [3]                     │
│▶   chilling 1.mp3                  │
│    chilling 2.mp3                  │
│    chilling 3.mp3                  │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│                                    │
│    create     modify     delete    │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace
