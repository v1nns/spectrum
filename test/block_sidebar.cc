#include <gmock/gmock-matchers.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

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
#include "model/application_error.h"
#include "view/base/keybinding.h"
#include "view/block/sidebar.h"
#include "view/block/sidebar_content/list_directory.h"
#include "view/block/sidebar_content/playlist_viewer.h"
#include "view/element/text_animation.h"
#include "view/element/util.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Not;
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

  //! Check if some file is highlighted (as playing) in ListDirectory
  bool IsFileHighlighted() {
    auto files = GetListDirectory();
    return files->curr_playing_.has_value() || files->menu_->actual().highlighted_.has_value();
  }

  //! Getter for files menu box from ListDirectory (only valid after rendering block)
  ftxui::Box GetFilesMenuBox() { return GetListDirectory()->menu_->Box(); }

  //! Getter for current dir from ListDirectory
  auto GetCurrentDir() -> std::filesystem::path { return GetListDirectory()->GetCurrentDir(); }

  //! Getter for filename from active entry in ListDirectory
  auto GetActiveFilename() -> std::filesystem::path {
    auto active = GetListDirectory()->menu_->GetActiveEntry();
    return active.has_value() ? active->filename() : std::filesystem::path{};
  }

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

  //! Check if some song is highlighted (as playing) in PlaylistViewer
  bool IsPlaylistSongHighlighted() {
    return GetPlaylistViewer()->menu_->actual().highlighted_.has_value();
  }

  //! Getter for playlists menu box from PlaylistViewer (only valid after rendering block)
  ftxui::Box GetPlaylistsMenuBox() { return GetPlaylistViewer()->menu_->Box(); }

  //! Check if there is an active entry in playlists menu from PlaylistViewer
  bool HasActivePlaylistEntry() { return GetPlaylistViewer()->menu_->GetActiveEntry().has_value(); }

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
  EXPECT_CALL(*dispatcher, SetApplicationError(Eq(error::kAccessDirFailed), _)).Times(0);

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
│  driver_ytdlp.cc                   │
│  general                           │
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
│  driver_ytdlp.cc                   │
│  general                           │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, RenderWithNarrowWidth) {
  // Sidebar does not have enough room for its maximum columns, so entries must be truncated on
  // the right side (and never horizontally scrolled, which used to hide the cursor prefix)
  screen = std::make_unique<ftxui::Screen>(24, 8);

  block->OnEvent(ftxui::Event::ArrowDown);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ F1:files  F2:playlist╮
│test                  │
│  ..                  │
│▶ audio_lyric_finder.c│
│  audio_player.cc     │
│  block_file_info.cc  │
│  block_main_content.c│
╰──────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NavigateWithAlternativeHomeEnd) {
  // Sequences sent by tmux (and other terminals) for End and Home keys
  auto end = interface::keybinding::Normalize(ftxui::Event::Special("\x1B[4~"));
  auto home = interface::keybinding::Normalize(ftxui::Event::Special("\x1B[1~"));

  EXPECT_EQ(end, ftxui::Event::End);
  EXPECT_EQ(home, ftxui::Event::Home);

  // Also check rxvt-style sequences
  EXPECT_EQ(interface::keybinding::Normalize(ftxui::Event::Special("\x1B[8~")), ftxui::Event::End);
  EXPECT_EQ(interface::keybinding::Normalize(ftxui::Event::Special("\x1B[7~")), ftxui::Event::Home);

  // Other events must remain untouched
  EXPECT_EQ(interface::keybinding::Normalize(ftxui::Event::ArrowDown), ftxui::Event::ArrowDown);
  EXPECT_EQ(interface::keybinding::Normalize(ftxui::Event::Character('4')),
            ftxui::Event::Character('4'));

  block->OnEvent(end);
  block->OnEvent(home);
  block->OnEvent(end);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("▶ "));
  EXPECT_THAT(rendered, Not(HasSubstr("▶ ..")));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, NavigateToMockDir) {
  block->OnEvent(ftxui::Event::End);
  block->OnEvent(ftxui::Event::ArrowUp);
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
│  stream_fetcher_mock.h             │
│  url_fetcher_mock.h                │
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
│  driver_ytdlp.cc                   │
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
│  driver_ytdlp.cc                   │
│  general                           │
│  middleware_media_controller.cc    │
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
│  stream_fetcher_mock.h             │
│  url_fetcher_mock.h                │
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
│  No matches                        │
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
│  driver_ytdlp.cc                   │
│  general                           │
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
│  driver_ytdlp.cc                   │
│  general                           │
╰────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, ClearSongInfoOnAllTabs) {
  std::filesystem::path file{LISTDIR_PATH + std::string("/audio_player.cc")};
  model::Playlists data{{model::Playlist{
      .index = 0,
      .name = "Chill mix",
      .songs = {model::Song{.filepath = file}},
  }}};

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillRepeatedly(DoAll(SetArgReferee<0>(data), Return(true)));
  EXPECT_CALL(*file_handler_mock_, SavePlaylists(_)).WillRepeatedly(Return(true));

  auto sidebar = std::static_pointer_cast<interface::Sidebar>(block);
  auto update_song = interface::CustomEvent::UpdateSongInfo(
      model::Song{.filepath = file, .playlist = "Chill mix"});
  auto clear_song = interface::CustomEvent::ClearSongInfo();

  // Load playlists, then show files tab again
  block->OnEvent(ftxui::Event::F2);
  block->OnEvent(ftxui::Event::F1);

  // Song from playlist is highlighted on both tabs
  sidebar->OnCustomEvent(update_song);
  ASSERT_TRUE(IsFileHighlighted());
  ASSERT_TRUE(IsPlaylistSongHighlighted());

  // Song stops while files tab is active: playlist tab must not keep highlighting it
  sidebar->OnCustomEvent(clear_song);
  EXPECT_FALSE(IsFileHighlighted());
  EXPECT_FALSE(IsPlaylistSongHighlighted());

  // Same thing while playlist tab is active: files tab must not keep highlighting it
  block->OnEvent(ftxui::Event::F2);
  sidebar->OnCustomEvent(update_song);
  ASSERT_TRUE(IsFileHighlighted());
  ASSERT_TRUE(IsPlaylistSongHighlighted());

  sidebar->OnCustomEvent(clear_song);
  EXPECT_FALSE(IsFileHighlighted());
  EXPECT_FALSE(IsPlaylistSongHighlighted());
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, PlayNextFileWhileShowingPlaylists) {
  std::filesystem::path file{LISTDIR_PATH + std::string("/audio_lyric_finder.cc")};
  std::filesystem::path next_file{LISTDIR_PATH + std::string("/audio_player.cc")};

  auto sidebar = std::static_pointer_cast<interface::Sidebar>(block);

  // File played from files tab, then user switches to playlist tab
  sidebar->OnCustomEvent(interface::CustomEvent::UpdateSongInfo(model::Song{.filepath = file}));

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_)).WillOnce(Return(false));
  block->OnEvent(ftxui::Event::F2);

  // When song finishes, files tab must still play the next file
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(next_file)))));

  sidebar->OnCustomEvent(interface::CustomEvent::UpdateSongState(
      model::Song::CurrentInformation{.state = model::Song::MediaState::Finished}));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, DoNotPlayNextFileAfterPlaylistSong) {
  std::filesystem::path file{LISTDIR_PATH + std::string("/audio_lyric_finder.cc")};

  auto sidebar = std::static_pointer_cast<interface::Sidebar>(block);

  // Song played from playlist (while showing files tab)
  sidebar->OnCustomEvent(interface::CustomEvent::UpdateSongInfo(
      model::Song{.filepath = file, .playlist = "Chill mix"}));

  // Player already takes care of playing next song from playlist
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::NotifyFileSelection)))
      .Times(0);

  sidebar->OnCustomEvent(interface::CustomEvent::UpdateSongState(
      model::Song::CurrentInformation{.state = model::Song::MediaState::Finished}));
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
│  driver_ytdlp.cc                   │
│  general                           │
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
│spectrum                            │)";

  // Instead of checking for the whole list, just check that changed the base directory
  EXPECT_THAT(rendered, HasSubstr(expected));

  // And that the directory we came from is selected
  EXPECT_THAT(rendered, HasSubstr("│▶ test "));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, SelectPreviousDirectoryAfterGoingUp) {
  // Enter "general" directory (found by search), then go back to parent directory using ".."
  std::string typed{"/general"};
  utils::QueueCharacterEvents(*block, typed);

  block->OnEvent(ftxui::Event::Return);
  EXPECT_EQ(GetCurrentDir().filename(), "general");

  block->OnEvent(ftxui::Event::Home);
  block->OnEvent(ftxui::Event::Return);
  EXPECT_EQ(GetCurrentDir().filename(), "test");

  // Cursor must be on the directory we came from, instead of ".."
  EXPECT_EQ(GetActiveFilename(), "general");

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("│▶ general "));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, ReloadDirectoryOnFocus) {
  // Create temporary directory with a few files
  auto dir = std::filesystem::temp_directory_path() / "spectrum_test_reload_on_focus";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directory(dir);
  utils::CreateEmptyFile(dir / "a.mp3");
  utils::CreateEmptyFile(dir / "c.mp3");

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_)).WillOnce(Return(false));
  block = ftxui::Make<interface::Sidebar>(dispatcher, dir.string(), file_handler_mock_);

  auto sidebar = std::static_pointer_cast<interface::Block>(block);
  sidebar->SetFocused(true);

  // Select "c.mp3" and remove focus from block
  block->OnEvent(ftxui::Event::End);
  sidebar->SetFocused(false);

  // Add a new file while block is not focused, list must not change
  utils::CreateEmptyFile(dir / "b.mp3");

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());
  EXPECT_THAT(rendered, Not(HasSubstr("b.mp3")));

  // After getting focus again, list must contain the new file and keep "c.mp3" selected
  sidebar->SetFocused(true);

  screen->Clear();
  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("│  b.mp3 "));
  EXPECT_THAT(rendered, HasSubstr("│▶ c.mp3 "));

  std::filesystem::remove_all(dir);
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, DimNonAudioFiles) {
  // Create temporary directory with audio and non-audio files
  auto dir = std::filesystem::temp_directory_path() / "spectrum_test_dim_non_audio";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directory(dir);
  utils::CreateEmptyFile(dir / "notes.txt");
  utils::CreateEmptyFile(dir / "song.FLAC");

  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_)).WillOnce(Return(false));
  block = ftxui::Make<interface::Sidebar>(dispatcher, dir.string(), file_handler_mock_);

  ftxui::Render(*screen, block->Render());

  // Check if first character from the given entry name is rendered as dimmed
  auto is_dimmed = [this](const std::string& name) {
    const int length = static_cast<int>(name.size());

    for (int y = 0; y < screen->dimy(); ++y) {
      for (int x = 0; x + length <= screen->dimx(); ++x) {
        bool match = true;
        for (int i = 0; i < length && match; ++i) {
          match = screen->PixelAt(x + i, y).character == name.substr(i, 1);
        }

        if (match) {
          return screen->PixelAt(x, y).dim;
        }
      }
    }

    ADD_FAILURE() << "Could not find entry " << name;
    return false;
  };

  EXPECT_TRUE(is_dimmed("notes.txt"));
  EXPECT_FALSE(is_dimmed("song.FLAC"));

  std::filesystem::remove_all(dir);
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
│  driver_ytdlp.cc                   │
│  general                           │
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
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  driver_ytdlp.cc                   │
│  general                           │
│  middleware_media_controller.cc    │
│  mock                              │
│  util_argparser.cc                 │
│  util_file_handler.cc              │
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
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  driver_ytdlp.cc                   │
│  general                           │
│  middleware_media_controller.cc    │
│  mock                              │
│  util_argparser.cc                 │
│  util_file_handler.cc              │
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
│  No matches                        │
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
│  No matches                        │
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
│  driver_fftw.cc                    │
│  driver_ytdlp.cc                   │
│  general                           │
│  middleware_media_controller.cc    │
│  mock                              │
│  util_argparser.cc                 │
│  util_file_handler.cc              │
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
│  driver_ytdlp.cc                   │
│  general                           │
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
  std::filesystem::path file{LISTDIR_PATH + std::string{"/util_file_handler.cc"}};
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
│  block_main_content.cc             │
│  block_media_player.cc             │
│  block_sidebar.cc                  │
│  CMakeLists.txt                    │
│  dialog_playlist.cc                │
│  driver_fftw.cc                    │
│  driver_ytdlp.cc                   │
│  general                           │
│  middleware_media_controller.cc    │
│  mock                              │
│  util_argparser.cc                 │
│▶ util_file_handler.cc              │
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

/**
 * @brief Tests with Sidebar class using a callback to check if file contains audio stream
 */
class SidebarAudioCheckTest : public ::SidebarTest {
 protected:
  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(38, 15);
    dispatcher = std::make_shared<EventDispatcherMock>();

    EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_)).WillOnce(Return(true));

    // Only these files are considered to contain an audio stream
    auto contains_audio = [](const util::File& file) {
      return file.filename() == "audio_player.cc" || file.filename() == "block_media_player.cc";
    };

    block = ftxui::Make<interface::Sidebar>(dispatcher, LISTDIR_PATH, file_handler_mock_,
                                            contains_audio);

    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);
  }
};

/* ********************************************************************************************** */

TEST_F(SidebarAudioCheckTest, SelectFileWithoutAudioStream) {
  // File without audio stream must not be sent to player (it would stop current song)
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(::testing::AnyNumber());
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::NotifyFileSelection)))
      .Times(0);

  EXPECT_CALL(*dispatcher,
              SetApplicationError(Eq(error::kFileNotSupported), StrEq("block_file_info.cc")));

  // Select "block_file_info.cc"
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::ArrowDown);
  block->OnEvent(ftxui::Event::Return);
}

/* ********************************************************************************************** */

TEST_F(SidebarAudioCheckTest, PlayNextFileSkippingFilesWithoutAudioStream) {
  InSequence seq;
  auto derived = GetListDirectory();

  // Simulate player sending event to update song info
  std::filesystem::path file{LISTDIR_PATH + std::string{"/audio_player.cc"}};
  derived->OnCustomEvent(interface::CustomEvent::UpdateSongInfo(model::Song{.filepath = file}));
  EXPECT_EQ(file, GetCurrentPlaying());

  // Next files ("block_file_info.cc" and "block_main_content.cc") do not contain audio stream
  std::filesystem::path next_file{LISTDIR_PATH + std::string{"/block_media_player.cc"}};

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::NotifyFileSelection),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<std::filesystem::path>(next_file)))))
      .Times(1);

  EXPECT_CALL(*dispatcher, SetApplicationError(_, _)).Times(0);

  // Simulate player sending event to notify that song has ended
  derived->OnCustomEvent(interface::CustomEvent::UpdateSongState(
      model::Song::CurrentInformation{.state = model::Song::MediaState::Finished}));
}

/* ********************************************************************************************** */

TEST_F(SidebarTest, MouseWheelOnMenus) {
  // Render block to calculate position of each element on screen
  ftxui::Render(*screen, block->Render());
  ftxui::Box box = GetFilesMenuBox();

  auto wheel = [](ftxui::Mouse::Button button, const ftxui::Box& box) {
    return ftxui::Event::Mouse("", ftxui::Mouse{.button = button,
                                                .motion = ftxui::Mouse::Pressed,
                                                .x = box.x_min + 1,
                                                .y = box.y_min + 1});
  };

  // Scroll down twice and up once on files list (starting from "..")
  EXPECT_TRUE(block->OnEvent(wheel(ftxui::Mouse::WheelDown, box)));
  EXPECT_TRUE(block->OnEvent(wheel(ftxui::Mouse::WheelDown, box)));
  EXPECT_TRUE(block->OnEvent(wheel(ftxui::Mouse::WheelUp, box)));

  EXPECT_THAT(GetActiveFilename(), Eq("audio_lyric_finder.cc"));

  // Scrolling on an empty list must not do anything
  model::Playlists data{};
  EXPECT_CALL(*file_handler_mock_, ParsePlaylists(_))
      .WillOnce(DoAll(SetArgReferee<0>(data), Return(true)));

  block->OnEvent(ftxui::Event::F2);
  screen->Clear();
  ftxui::Render(*screen, block->Render());

  box = GetPlaylistsMenuBox();
  block->OnEvent(wheel(ftxui::Mouse::WheelDown, box));
  block->OnEvent(wheel(ftxui::Mouse::WheelUp, box));

  EXPECT_FALSE(HasActivePlaylistEntry());
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
│  No playlists, press c to create   │
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
│▶ Chill mix really long and the coo │
│    chilling 1.mp3                  │
│    chilling 3.mp3                  │
│    chilling with a really long nam │
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
│▶  mix really long and the coolest  │
│    chilling 1.mp3                  │
│    chilling 3.mp3                  │
│    chilling with a really long nam │
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
│  Chill mix really long and the coo │
│▶   chilling 1.mp3                  │
│    chilling 3.mp3                  │
│    chilling with a really long nam │
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
│▶   chilling with a really long nam │
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
│    chilling with a really long nam │
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
      .other_names = {"Chill mix", "Lofi"},
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
      .other_names = {"Lofi"},
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
│  No playlists, press c to create   │
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
│  No playlists, press c to create   │
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

/* ********************************************************************************************** */

//! Tests for shortening current directory shown as title in files tab
TEST(ShortenPathTest, PathThatFits) {
  EXPECT_THAT(interface::shorten_path("/home/user/music", 16), StrEq("/home/user/music"));
}

TEST(ShortenPathTest, StartFromDirectorySeparator) {
  EXPECT_THAT(interface::shorten_path("/home/user/collection/electronic/artists/aphex", 30),
              StrEq(".../electronic/artists/aphex"));
}

TEST(ShortenPathTest, LastDirectoryLongerThanColumns) {
  EXPECT_THAT(interface::shorten_path("/home/user/a_really_long_folder_name_for_an_album", 20),
              StrEq("...name_for_an_album"));
}

TEST(ShortenPathTest, MultiByteAndFullWidthCharacters) {
  // Each accented letter takes a single column (and must never be split)
  EXPECT_THAT(interface::shorten_path("/home/Músicas clássicas", 12), StrEq("...clássicas"));

  // Full-width characters take two columns each
  EXPECT_THAT(interface::shorten_path("/home/音楽音楽音楽", 9), StrEq("...楽音楽"));
}

TEST(ShortenPathTest, NotEnoughColumns) {
  EXPECT_THAT(interface::shorten_path("/home/user", 2), StrEq(".."));
  EXPECT_THAT(interface::shorten_path("/home/user", 0), StrEq(""));
}

/* ********************************************************************************************** */

//! Tests for text animation used by menus to show long entries
TEST(TextAnimationTest, MoveWholeCharacterOnEachStep) {
  std::atomic<int> updates = 0;

  interface::TextAnimation animation;
  animation.cb_update = [&updates] { updates++; };

  // Wait until animation has moved text the given number of steps (or timeout)
  auto wait_steps = [&updates](int steps) {
    for (int i = 0; i < 100 && updates < steps; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };

  animation.Start("ção");
  EXPECT_THAT(animation.GetText(), StrEq("ção "));

  // Each step must move a whole character, even when it uses more than one byte
  wait_steps(1);
  EXPECT_THAT(animation.GetText(), StrEq("ão ç"));

  wait_steps(2);
  EXPECT_THAT(animation.GetText(), StrEq("o çã"));

  animation.Stop();
}

}  // namespace
