#include <gmock/gmock-matchers.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include "general/dialog.h"
#include "general/utils.h"
#include "gmock/gmock.h"
#include "mock/event_dispatcher_mock.h"
#include "mock/file_handler_mock.h"
#include "model/playlist.h"
#include "model/playlist_operation.h"
#include "model/question_data.h"
#include "model/settings.h"
#include "util/file_handler.h"
#include "view/base/keybinding.h"
#include "view/element/error_dialog.h"
#include "view/element/help_dialog.h"
#include "view/element/playlist_dialog.h"
#include "view/element/question_dialog.h"
#include "view/element/style.h"
#include "view/element/theme_picker.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrEq;
using ::testing::VariantWith;

/**
 * @brief Tests with PlaylistDialog class
 */
class PlaylistDialogTest : public ::DialogTest {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(size.dimx, size.dimy);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create playlist dialog with test directory as base dir
    dialog = std::make_unique<interface::PlaylistDialog>(
        dispatcher, contains_audio_cb.AsStdFunction(), LISTDIR_PATH);
  }

  //! Getter for PlaylistDialog (downcasting)
  auto GetPlaylistDialog() -> interface::PlaylistDialog* {
    return reinterpret_cast<interface::PlaylistDialog*>(dialog.get());
  }

  //! Getter for rendered screen (besides filtering ANSI commands, should also trim empty spaces)
  std::string GetRenderedScreen() {
    std::string filtered = utils::FilterAnsiCommands(screen->ToString());
    return utils::FilterEmptySpaces(filtered);
  }

  //!< Screen dimension (already considering size restraints from dialog)
  ftxui::Dimensions size = ftxui::Dimensions{.dimx = 130, .dimy = 40};

  //! Mock function to check for audio stream on given file
  MockFunction<bool(const util::File&)> contains_audio_cb;
};

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, InitialRenderWithCreate) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  };

  GetPlaylistDialog()->Open(operation);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ───────────────────╮      ║
║      │test                          ││                              │      ║
║      │▶ ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Add a song to save           ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, InitialRenderWithModify) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Chill mix",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "chilling 1.mp3"},
                                                 model::Song{.filepath = "chilling 2.mp3"},
                                                 model::Song{.filepath = "chilling 3.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Modify Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ Chill mix ───────────────────╮      ║
║      │test                          ││▶ chilling 1.mp3              │      ║
║      │▶ ..                          ││  chilling 2.mp3              │      ║
║      │  audio_lyric_finder.cc       ││  chilling 3.mp3              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │                              ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, NavigateSearchAndCreatePlaylist) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  };

  GetPlaylistDialog()->Open(operation);

  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).Times(2).WillRepeatedly(Return(true));

  // Navigate, add one file, then search and add another one
  std::string typed{"jjj /fftw"};
  utils::QueueCharacterEvents(*dialog, typed);

  // Setup expectation for event enabling global mode again
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);

  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ───────────────────╮      ║
║      │test                          ││▶ block_file_info.cc          │      ║
║      │  ..                          ││  driver_fftw.cc              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │▶ block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Name it to save (r)          ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));

  // Add one more, change focus to playlist, and remove penultimate entry
  typed = "j lj ";
  utils::QueueCharacterEvents(*dialog, typed);

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));

  rendered = GetRenderedScreen();

  expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ─────────[r:rename]╮      ║
║      │test                          ││  block_file_info.cc          │      ║
║      │  ..                          ││▶ block_main_content.cc       │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │▶ block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Name it to save (r)          ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Set a name to playlist and save it
  typed = "rsummer hits";
  utils::QueueCharacterEvents(*dialog, typed);

  dialog->OnEvent(ftxui::Event::Return);

  // Setup expectation for event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .WillOnce(Invoke([](const interface::CustomEvent event) {
        // Check for playlist content (but we do not want to check for complete song filepath)
        auto content = event.GetContent<model::Playlist>();
        EXPECT_THAT(content.name, "summer hits");
        EXPECT_THAT(content.songs.size(), Eq(2));
      }));

  dialog->OnEvent(ftxui::Event::Character('s'));

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));

  rendered = GetRenderedScreen();

  expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ summer hits ───────[r:rename]╮      ║
║      │test                          ││  block_file_info.cc          │      ║
║      │  ..                          ││▶ block_main_content.cc       │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │▶ block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Saved ✓                      ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, CancelRenamingAndCreateNewPlaylistOnlyAfterValidName) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  };

  GetPlaylistDialog()->Open(operation);

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));

  // Focus playlist menu, add a song and focus playlist menu
  std::string typed{"jjjjj l"};
  utils::QueueCharacterEvents(*dialog, typed);

  // Enter on rename mode and cancel it
  dialog->OnEvent(ftxui::Event::Character('r'));
  dialog->OnEvent(ftxui::Event::Escape);

  // Save operation will not work while playlist has not a name
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(0);

  dialog->OnEvent(ftxui::Event::Character('s'));

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ─────────[r:rename]╮      ║
║      │test                          ││▶ block_media_player.cc       │      ║
║      │  ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │▶ block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Name it to save (r)          ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Put some name on the playlist
  typed = "ronly the best";
  utils::QueueCharacterEvents(*dialog, typed);

  dialog->OnEvent(ftxui::Event::Return);

  // Setup expectation for event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .WillOnce(Invoke([](const interface::CustomEvent& event) {
        // Check event content
        const auto& playlist_content = event.GetContent<model::Playlist>();
        EXPECT_THAT(playlist_content.name, StrEq("only the best"));
        EXPECT_THAT(playlist_content.songs.begin()->filepath.filename().string(),
                    StrEq("block_media_player.cc"));
      }));

  dialog->OnEvent(ftxui::Event::Character('s'));

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));

  rendered = GetRenderedScreen();

  expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ only the best ─────[r:rename]╮      ║
║      │test                          ││▶ block_media_player.cc       │      ║
║      │  ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │▶ block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Saved ✓                      ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, CancelRenamingAndRemoveOneSong) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Melodic House",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Crazy hit.mp3"},
                                                 model::Song{.filepath = "Crazy frog.mp3"},
                                                 model::Song{.filepath = "Crazy love.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Focus playlist menu, rename and cancel
  std::string typed{"lr"};
  utils::QueueCharacterEvents(*dialog, typed);

  dialog->OnEvent(ftxui::Event::Escape);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Modify Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ Melodic House ─────[r:rename]╮      ║
║      │test                          ││▶ Crazy hit.mp3               │      ║
║      │▶ ..                          ││  Crazy frog.mp3              │      ║
║      │  audio_lyric_finder.cc       ││  Crazy love.mp3              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │                              ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  // Search for last entry, remove it and save playlist
  typed = "/love";
  utils::QueueCharacterEvents(*dialog, typed);

  // Setup expectation for event enabling global mode again
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);

  dialog->OnEvent(ftxui::Event::Return);

  // Use existent playlist to create expectation
  model::Playlist expected_playlist = *operation.playlist;
  expected_playlist.songs.pop_back();

  // Setup expectation for event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::SavePlaylistsToFile),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::Playlist>(expected_playlist)))));

  dialog->OnEvent(ftxui::Event::Character('s'));

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));

  rendered = GetRenderedScreen();

  expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Modify Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ Melodic House ─────[r:rename]╮      ║
║      │test                          ││▶ Crazy hit.mp3               │      ║
║      │▶ ..                          ││  Crazy frog.mp3              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Saved ✓                      ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, AddThenRemoveSongFromExistentPlaylist) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Melodic House",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Crazy hit.mp3"},
                                                 model::Song{.filepath = "Crazy frog.mp3"},
                                                 model::Song{.filepath = "Crazy love.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));

  // Add random file, focus playlist menu and remove new entry
  std::string typed{"jjj ljjj "};
  utils::QueueCharacterEvents(*dialog, typed);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Modify Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ Melodic House ─────[r:rename]╮      ║
║      │test                          ││  Crazy hit.mp3               │      ║
║      │  ..                          ││  Crazy frog.mp3              │      ║
║      │  audio_lyric_finder.cc       ││▶ Crazy love.mp3              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │▶ block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │                              ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation that event to save playlist in JSON file should not be sent
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(0);

  // Make an attempt to save playlist, but this should not work
  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, SwitchMenusWithTab) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Melodic House",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Crazy hit.mp3"},
                                                 model::Song{.filepath = "Crazy frog.mp3"},
                                                 model::Song{.filepath = "Crazy love.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Focus playlist menu and navigate on it
  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(ftxui::Event::Character('j'));

  // Focus files menu again (wrapping around) and navigate on it
  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(ftxui::Event::Character('j'));
  dialog->OnEvent(ftxui::Event::Character('j'));

  // Focus playlist menu using reverse direction and navigate on it
  dialog->OnEvent(ftxui::Event::TabReverse);
  dialog->OnEvent(ftxui::Event::Character('j'));

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Modify Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ Melodic House ─────[r:rename]╮      ║
║      │test                          ││  Crazy hit.mp3               │      ║
║      │  ..                          ││  Crazy frog.mp3              │      ║
║      │  audio_lyric_finder.cc       ││▶ Crazy love.mp3              │      ║
║      │▶ audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │                              ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, CloseWithEscape) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Melodic House",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Crazy hit.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Escape while searching on files menu only exits search mode
  dialog->OnEvent(ftxui::Event::Character('/'));
  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_TRUE(dialog->IsVisible());

  // Escape while renaming playlist only exits edit mode
  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(ftxui::Event::Character('r'));
  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_TRUE(dialog->IsVisible());

  // Otherwise, escape closes dialog
  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_FALSE(dialog->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, ShowMessageAfterSave) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Melodic House",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Crazy hit.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));

  // Add random file to playlist
  std::string typed{"jjj "};
  utils::QueueCharacterEvents(*dialog, typed);

  // Setup expectation for event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)));

  // Save playlist, dialog should stay open and show confirmation message
  dialog->OnEvent(ftxui::Event::Character('s'));
  EXPECT_TRUE(dialog->IsVisible());

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("Saved ✓"));

  // Close and open dialog again, message should not be displayed anymore
  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_FALSE(dialog->IsVisible());

  GetPlaylistDialog()->Open(operation);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), Not(HasSubstr("Saved ✓")));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, AddYoutubeUrlAndSave) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create};

  GetPlaylistDialog()->Open(operation);

  // Show URL input and add a YouTube URL
  dialog->OnEvent(ftxui::Event::F2);
  utils::QueueCharacterEvents(*dialog, "https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ───────────────────╮      ║
║      │                              ││▶ [yt] youtu.be/dQw4w9WgXcQ   │      ║
║      │ Paste a YouTube URL:         ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │ Return: add                  ││                              │      ║
║      │ Escape: clear                ││                              │      ║
║      │                              ││                              │      ║
║      │ ✓ Added to playlist          ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Name it to save (r)          ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Set a name to playlist and save it
  dialog->OnEvent(ftxui::Event::Tab);
  utils::QueueCharacterEvents(*dialog, "rmix");
  dialog->OnEvent(ftxui::Event::Return);

  // Setup expectation for event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .WillOnce(Invoke([](const interface::CustomEvent event) {
        auto content = event.GetContent<model::Playlist>();
        EXPECT_THAT(content.name, "mix");
        ASSERT_THAT(content.songs.size(), Eq(1));
        ASSERT_TRUE(content.songs.front().stream_info.has_value());
        EXPECT_THAT(content.songs.front().stream_info->base_url,
                    StrEq("https://www.youtube.com/watch?v=dQw4w9WgXcQ"));
      }));

  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, CannotAddUrlWithoutYtDlp) {
  // Songs from URL depend on yt-dlp, which is not available
  dialog = std::make_unique<interface::PlaylistDialog>(
      dispatcher, contains_audio_cb.AsStdFunction(), LISTDIR_PATH, [] { return false; });

  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create};
  GetPlaylistDialog()->Open(operation);

  // Label tells user what is missing, and URL is not added to playlist
  dialog->OnEvent(ftxui::Event::F2);
  utils::QueueCharacterEvents(*dialog, "https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ───────────────────╮      ║
║      │                              ││                              │      ║
║      │ yt-dlp not installed         ││                              │      ║
║      │                              ││                              │      ║
║      │ ube.com/watch?v=dQw4w9WgXcQ  ││                              │      ║
║      │                              ││                              │      ║
║      │ Return: add                  ││                              │      ║
║      │ Escape: clear                ││                              │      ║
║      │                              ││                              │      ║
║      │ ✗ yt-dlp not found           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Add a song to save           ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, ImportSongsFromYoutubePlaylist) {
  const std::string playlist_url{"https://www.youtube.com/playlist?list=PLabcdefghijklmnop"};

  // Playlist extraction is blocked until test releases it, so pending state can be checked
  std::promise<void> release;
  std::shared_future<void> released = release.get_future().share();

  auto fetch = [&](const std::string& url, std::vector<model::Song>& songs,
                   const std::atomic<bool>*) {
    EXPECT_THAT(url, StrEq(playlist_url));
    released.wait();

    auto song = [](const std::string& title, const std::string& id) {
      return model::Song{
          .title = title,
          .stream_info = model::StreamInfo{.base_url = "https://www.youtube.com/watch?v=" + id}};
    };

    songs = {song("First song", "aaaaaaaaaaa"), song("Second song", "bbbbbbbbbbb"),
             song("Already added", "dQw4w9WgXcQ")};
    return error::kSuccess;
  };

  dialog = std::make_unique<interface::PlaylistDialog>(
      dispatcher, contains_audio_cb.AsStdFunction(), LISTDIR_PATH, nullptr, fetch);

  // Import finishes in another thread, which asks for a refresh to add songs to playlist
  std::promise<void> refreshed;
  std::atomic<bool> notified = false;
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::Refresh)))
      .WillRepeatedly(Invoke([&](const interface::CustomEvent&) {
        if (!notified.exchange(true)) refreshed.set_value();
      }));

  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create};
  GetPlaylistDialog()->Open(operation);

  // Add a single song first
  dialog->OnEvent(ftxui::Event::F2);
  utils::QueueCharacterEvents(*dialog, "https://www.youtube.com/watch?v=dQw4w9WgXcQ");
  dialog->OnEvent(ftxui::Event::Return);

  // Then import a whole playlist
  utils::QueueCharacterEvents(*dialog, playlist_url);
  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("… Importing playlist"));

  // Another URL cannot be submitted while importing
  utils::QueueCharacterEvents(*dialog, "x");
  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("✗ Already importing"));

  release.set_value();
  ASSERT_EQ(refreshed.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);

  // Refresh is received by dialog as an event (from terminal)
  dialog->OnEvent(ftxui::Event::Custom);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();
  EXPECT_THAT(rendered, HasSubstr("✓ Added 2 songs, 1 skipped"));
  EXPECT_THAT(rendered, HasSubstr("First song"));
  EXPECT_THAT(rendered, HasSubstr("Second song"));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, CancelPlaylistImportWhenClosingDialog) {
  std::promise<void> started;
  std::atomic<bool> canceled = false;

  // Extraction runs until it gets canceled
  auto fetch = [&](const std::string&, std::vector<model::Song>&, const std::atomic<bool>* cancel) {
    started.set_value();
    while (!*cancel) std::this_thread::sleep_for(std::chrono::milliseconds(5));

    canceled = true;
    return error::kStreamFetchFailed;
  };

  dialog = std::make_unique<interface::PlaylistDialog>(
      dispatcher, contains_audio_cb.AsStdFunction(), LISTDIR_PATH, nullptr, fetch);

  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create};
  GetPlaylistDialog()->Open(operation);

  dialog->OnEvent(ftxui::Event::F2);
  utils::QueueCharacterEvents(*dialog, "https://www.youtube.com/playlist?list=PLabcdefghijklmnop");
  dialog->OnEvent(ftxui::Event::Return);

  ASSERT_EQ(started.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready);

  // Closing dialog cancels import (and waits for it)
  dialog->Close();
  EXPECT_TRUE(canceled);
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, CannotImportPlaylistWithoutYtDlp) {
  MockFunction<error::Code(const std::string&, std::vector<model::Song>&, const std::atomic<bool>*)>
      fetch;
  EXPECT_CALL(fetch, Call).Times(0);

  dialog = std::make_unique<interface::PlaylistDialog>(
      dispatcher, contains_audio_cb.AsStdFunction(), LISTDIR_PATH, [] { return false; },
      fetch.AsStdFunction());

  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create};
  GetPlaylistDialog()->Open(operation);

  dialog->OnEvent(ftxui::Event::F2);
  utils::QueueCharacterEvents(*dialog, "https://www.youtube.com/playlist?list=PLabcdefghijklmnop");
  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("✗ yt-dlp not found"));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RejectInvalidAndDuplicatedUrl) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Modify,
      .playlist =
          model::Playlist{
              .index = 0,
              .name = "Melodic House",
              .songs =
                  {
                      model::Song{
                          .stream_info =
                              model::StreamInfo{.base_url = "https://youtu.be/dQw4w9WgXcQ"}},
                  },
          },
  };

  GetPlaylistDialog()->Open(operation);
  dialog->OnEvent(ftxui::Event::F2);

  // URL from another website
  utils::QueueCharacterEvents(*dialog, "https://vimeo.com/123");
  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("✗ Not a YouTube URL"));

  // URL already in playlist
  dialog->OnEvent(ftxui::Event::Escape);
  utils::QueueCharacterEvents(*dialog, "https://youtu.be/dQw4w9WgXcQ");
  dialog->OnEvent(ftxui::Event::Return);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("✗ Already in playlist"));

  // Playlist was not modified, so it cannot be saved
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(0);

  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, TypeKeybindingsIntoUrlInput) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create};

  GetPlaylistDialog()->Open(operation);
  dialog->OnEvent(ftxui::Event::F2);

  // Keys to close dialog, save playlist and navigate are typed as part of the URL
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(0);

  utils::QueueCharacterEvents(*dialog, "qshjkl");
  EXPECT_TRUE(dialog->IsVisible());

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("qshjkl"));

  // First escape clears text, second one closes dialog
  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_TRUE(dialog->IsVisible());

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), Not(HasSubstr("qshjkl")));

  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_FALSE(dialog->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, SwitchBetweenFilesAndUrlInput) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create};

  GetPlaylistDialog()->Open(operation);

  // Show URL input, then files again
  dialog->OnEvent(ftxui::Event::F2);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("Paste a YouTube URL:"));

  dialog->OnEvent(ftxui::Event::F1);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();
  EXPECT_THAT(rendered, Not(HasSubstr("Paste a YouTube URL:")));
  EXPECT_THAT(rendered, HasSubstr("audio_player.cc"));

  // Files menu is focused again, so it is possible to add a file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));
  utils::QueueCharacterEvents(*dialog, "jj ");

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ audio_player.cc"));

  // Dialog always opens showing files
  dialog->OnEvent(ftxui::Event::F2);
  dialog->OnEvent(ftxui::Event::Escape);
  GetPlaylistDialog()->Open(operation);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), Not(HasSubstr("Paste a YouTube URL:")));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RenameStartsAtEndAndEscapeCancels) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Modify,
      .playlist = model::Playlist{.index = 0,
                                  .name = "Lofi",
                                  .songs = {model::Song{.filepath = "Love song.mp3"}}},
  };

  GetPlaylistDialog()->Open(operation);

  // Focus playlist menu, type something and cancel it
  dialog->OnEvent(ftxui::Event::Tab);
  utils::QueueCharacterEvents(*dialog, "rX");

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("╭ LofiX "));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("[Escape:cancel]"));

  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_TRUE(dialog->IsVisible());

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("╭ Lofi ─"));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("[r:rename]"));

  // Rename again, now appending text to the end of name
  utils::QueueCharacterEvents(*dialog, "r beats");
  dialog->OnEvent(ftxui::Event::Return);

  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .WillOnce(Invoke([](const interface::CustomEvent event) {
        EXPECT_THAT(event.GetContent<model::Playlist>().name, StrEq("Lofi beats"));
      }));

  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RenameWithAccentedCharacters) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Modify,
      .playlist = model::Playlist{.index = 0,
                                  .name = "",
                                  .songs = {model::Song{.filepath = "Love song.mp3"}}},
  };

  GetPlaylistDialog()->Open(operation);

  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(ftxui::Event::Character('r'));

  // Type multi-byte characters and erase some of them
  for (const auto& character : {"M", "ú", "s", "i", "c", "a", "s"}) {
    dialog->OnEvent(ftxui::Event::Character(character));
  }

  dialog->OnEvent(ftxui::Event::Backspace);
  dialog->OnEvent(ftxui::Event::Backspace);

  // Delete first character (cursor must stay at the beginning) and type it again
  dialog->OnEvent(ftxui::Event::Home);
  dialog->OnEvent(ftxui::Event::Delete);
  dialog->OnEvent(ftxui::Event::Character("m"));
  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("╭ músic ─"));

  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .WillOnce(Invoke([](const interface::CustomEvent event) {
        EXPECT_THAT(event.GetContent<model::Playlist>().name, StrEq("músic"));
      }));

  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RenameDeletingWords) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Modify,
      .playlist = model::Playlist{.index = 0,
                                  .name = "Músicas para codar",
                                  .songs = {model::Song{.filepath = "Love song.mp3"}}},
  };

  GetPlaylistDialog()->Open(operation);

  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(ftxui::Event::Character('r'));

  // Delete words using all supported keys (separators between words are deleted along with them)
  dialog->OnEvent(interface::keybinding::Navigation::CtrlW);
  dialog->OnEvent(interface::keybinding::Navigation::AltBackspace);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("Músicas "));
  EXPECT_THAT(GetRenderedScreen(), Not(HasSubstr("para")));

  dialog->OnEvent(interface::keybinding::Navigation::CtrlBackspace);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("type a name"));

  // Cancel it, so name is restored
  dialog->OnEvent(ftxui::Event::Escape);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("╭ Músicas para codar ─"));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RejectEmptyAndDuplicatedName) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Modify,
      .playlist = model::Playlist{.index = 0,
                                  .name = "Lofi",
                                  .songs = {model::Song{.filepath = "Love song.mp3"}}},
      .other_names = {"Chill"},
  };

  GetPlaylistDialog()->Open(operation);

  // Erase whole name and try to confirm it
  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(ftxui::Event::Character('r'));
  for (int i = 0; i < 4; ++i) dialog->OnEvent(ftxui::Event::Backspace);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("type a name"));

  utils::QueueCharacterEvents(*dialog, "   ");
  dialog->OnEvent(ftxui::Event::Return);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("✗ Name can't be empty"));

  // Name used by another playlist (spaces around it are ignored)
  utils::QueueCharacterEvents(*dialog, "Chill ");
  dialog->OnEvent(ftxui::Event::Return);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("✗ Name already used"));

  // Still editing, so a valid name can be typed
  utils::QueueCharacterEvents(*dialog, "out");
  dialog->OnEvent(ftxui::Event::Return);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();
  EXPECT_THAT(rendered, Not(HasSubstr("✗")));

  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .WillOnce(Invoke([](const interface::CustomEvent event) {
        EXPECT_THAT(event.GetContent<model::Playlist>().name, StrEq("Chill out"));
      }));

  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, ReloadFilesOnOpen) {
  // Create temporary directory with a single file
  auto dir = std::filesystem::temp_directory_path() / "spectrum_test_reload_on_open";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directory(dir);
  utils::CreateEmptyFile(dir / "first.mp3");

  // Use it as working directory too (dialog used to read files again only when paths differed)
  auto old_working_dir = std::filesystem::current_path();
  std::filesystem::current_path(dir);

  dialog = std::make_unique<interface::PlaylistDialog>(
      dispatcher, contains_audio_cb.AsStdFunction(), dir.string());

  const model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  };

  // Open and close dialog, then add a new file
  GetPlaylistDialog()->Open(operation);
  dialog->OnEvent(ftxui::Event::Character('q'));

  utils::CreateEmptyFile(dir / "second.mp3");

  // New file must be listed after opening dialog again
  GetPlaylistDialog()->Open(operation);

  ftxui::Render(*screen, dialog->Render(size));
  const std::string rendered = GetRenderedScreen();

  EXPECT_THAT(rendered, HasSubstr("first.mp3"));
  EXPECT_THAT(rendered, HasSubstr("second.mp3"));

  std::filesystem::current_path(old_working_dir);
  std::filesystem::remove_all(dir);
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RenameExistentPlaylist) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Lofi",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Love song.mp3"},
                                                 model::Song{.filepath = "Reggae wubba dubba.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Focus playlist menu and enable renaming mode
  std::string typed{"lr"};
  utils::QueueCharacterEvents(*dialog, typed);

  dialog->OnEvent(ftxui::Event::Home);

  // Add random preffix to playlist name
  typed = "not so ";
  utils::QueueCharacterEvents(*dialog, typed);

  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Modify Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ not so Lofi ───────[r:rename]╮      ║
║      │test                          ││▶ Love song.mp3               │      ║
║      │▶ ..                          ││  Reggae wubba dubba.mp3      │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │                              ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Use existent playlist to create expectation
  model::Playlist expected_playlist = *operation.playlist;
  expected_playlist.name = "not so Lofi";

  // Setup expectation for event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::SavePlaylistsToFile),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::Playlist>(expected_playlist)))));

  // Make an attempt to save playlist, but this should not work
  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, AttemptToCreateEmptyPlaylist) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create,
                                     .playlist = model::Playlist{}};

  GetPlaylistDialog()->Open(operation);

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(false));

  // Attempt to add a new entry
  std::string typed{"jjj "};
  utils::QueueCharacterEvents(*dialog, typed);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ───────────────────╮      ║
║      │test                          ││                              │      ║
║      │  ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │▶ block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Add a song to save           ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation that event to save playlist in JSON file should not be sent
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(0);

  // Make an attempt to save playlist, but this should not work
  dialog->OnEvent(ftxui::Event::Character('s'));

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));

  // Attempt to add a new entry
  typed = "j ";
  utils::QueueCharacterEvents(*dialog, typed);

  // Redraw element on screen
  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));

  rendered = GetRenderedScreen();

  expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ───────────────────╮      ║
║      │test                          ││▶ block_main_content.cc       │      ║
║      │  ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │▶ block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Name it to save (r)          ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RenameWithABiggerName) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create,
                                     .playlist = model::Playlist{}};

  GetPlaylistDialog()->Open(operation);

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));

  // Add a new entry
  std::string typed{"jjjjj lronceuponatimetherewasanepicplaylist"};
  utils::QueueCharacterEvents(*dialog, typed);

  // Apply new name
  dialog->OnEvent(ftxui::Event::Return);

  // Setup expectation for event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .WillOnce(Invoke([](const interface::CustomEvent event) {
        // Check for playlist content (but we do not want to check for complete song filepath)
        auto content = event.GetContent<model::Playlist>();
        EXPECT_THAT(content.name, "onceuponatimetherewasanepicplaylist");
        EXPECT_THAT(content.songs.size(), Eq(1));
      }));

  // Save playlist
  dialog->OnEvent(ftxui::Event::Character('s'));

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ onceuponatimetherewasanepicp ╮      ║
║      │test                          ││▶ block_media_player.cc       │      ║
║      │  ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │▶ block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Saved ✓                      ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, SendNonEmptyPlaylistWithCreate) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Reggae roots",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Love song.m4a"},
                                                 model::Song{.filepath = "Reggae wubba dubba.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Create Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ───────────────────╮      ║
║      │test                          ││                              │      ║
║      │▶ ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Add a song to save           ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, SendEmptyPlaylistWithModify) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 1,
                                     }};

  GetPlaylistDialog()->Open(operation);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Modify Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ <unnamed> ───────────────────╮      ║
║      │test                          ││                              │      ║
║      │▶ ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Add a song to save           ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Setup expectation for event disabling global mode
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::DisableGlobalEvent)))
      .Times(1);

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));

  // Focus an entry
  std::string typed{"/util"};
  utils::QueueCharacterEvents(*dialog, typed);

  // Setup expectation for event enabling global mode again
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::EnableGlobalEvent)))
      .Times(1);

  // Add new entry
  dialog->OnEvent(ftxui::Event::Return);

  // Without a playlist name, dialog will not send event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(0);

  // Save playlist
  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RemoveLastSongAndSave) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Chill piano",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Crazy piano solo.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Focus playlist menu and remove last song
  std::string typed{"l "};
  utils::QueueCharacterEvents(*dialog, typed);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  std::string expected = R"(
╔════════════════════════════════════════════════════════════════════════════╗
║                                                                            ║
║                              Modify Playlist                               ║
║                                                                            ║
║      ╭ F1:files  F2:youtube ────────╮╭ Chill piano ───────[r:rename]╮      ║
║      │test                          ││                              │      ║
║      │▶ ..                          ││                              │      ║
║      │  audio_lyric_finder.cc       ││                              │      ║
║      │  audio_player.cc             ││                              │      ║
║      │  block_file_info.cc          ││                              │      ║
║      │  block_main_content.cc       ││                              │      ║
║      │  block_media_player.cc       ││                              │      ║
║      │  block_sidebar.cc            ││                              │      ║
║      │  CMakeLists.txt              ││                              │      ║
║      │  dialog_playlist.cc          ││                              │      ║
║      │  driver_fftw.cc              ││                              │      ║
║      │  driver_ytdlp.cc             ││                              │      ║
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │  util_file_handler.cc        ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
║      ╰──────────────────────────────╯╰──────────────────────────────╯      ║
║                              ┌──────────────┐                              ║
║                              │     Save     │ Add a song to save           ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Without a song, dialog will not send event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(0);

  // Save playlist
  dialog->OnEvent(ftxui::Event::Character('s'));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RemoveSongsWithDedicatedKeys) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Melodic House",
                                         .songs =
                                             {
                                                 model::Song{.filepath = "Crazy hit.mp3"},
                                                 model::Song{.filepath = "Crazy frog.mp3"},
                                                 model::Song{.filepath = "Crazy love.mp3"},
                                             },
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Dedicated keys do nothing while files menu is focused
  dialog->OnEvent(interface::keybinding::Playlist::RemoveSong);
  dialog->OnEvent(interface::keybinding::Navigation::Delete);

  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  EXPECT_THAT(rendered, HasSubstr("Crazy hit.mp3"));
  EXPECT_THAT(rendered, HasSubstr("Crazy frog.mp3"));
  EXPECT_THAT(rendered, HasSubstr("Crazy love.mp3"));

  // Focus playlist menu, remove first song, then the one after the next
  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(interface::keybinding::Playlist::RemoveSong);
  dialog->OnEvent(interface::keybinding::Navigation::Down);
  dialog->OnEvent(interface::keybinding::Navigation::Delete);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  rendered = GetRenderedScreen();

  EXPECT_THAT(rendered, Not(HasSubstr("Crazy hit.mp3")));
  EXPECT_THAT(rendered, HasSubstr("Crazy frog.mp3"));
  EXPECT_THAT(rendered, Not(HasSubstr("Crazy love.mp3")));

  // Setup expectation for event to save playlist without removed songs
  model::Playlist expected_playlist{
      .index = 0,
      .name = "Melodic House",
      .songs = {model::Song{.index = 1, .filepath = "Crazy frog.mp3"}},
  };

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::SavePlaylistsToFile),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::Playlist>(expected_playlist)))));

  dialog->OnEvent(interface::keybinding::Playlist::Save);
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, TypeRemoveKeyWhileSearchingOrRenaming) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Mix",
                                         .songs = {model::Song{.filepath = "dance.mp3"}},
                                     }};

  GetPlaylistDialog()->Open(operation);

  // Focus playlist menu and search for song using a text with the key to remove song
  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(interface::keybinding::Navigation::EnableSearch);
  dialog->OnEvent(interface::keybinding::Playlist::RemoveSong);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("dance.mp3"));

  // Leave search mode, and type the same key while renaming playlist
  dialog->OnEvent(ftxui::Event::Escape);
  dialog->OnEvent(interface::keybinding::Playlist::Rename);
  dialog->OnEvent(interface::keybinding::Playlist::RemoveSong);
  dialog->OnEvent(ftxui::Event::Return);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  EXPECT_THAT(rendered, HasSubstr("Mixd"));
  EXPECT_THAT(rendered, HasSubstr("dance.mp3"));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, ShowRemoveHintWhenThereIsEnoughSpace) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Mix",
                                         .songs = {model::Song{.filepath = "dance.mp3"}},
                                     }};

  GetPlaylistDialog()->Open(operation);
  dialog->OnEvent(ftxui::Event::Tab);

  // Not enough columns on pane border for both hints, so only the one to rename is shown
  ftxui::Render(*screen, dialog->Render(size));
  std::string rendered = GetRenderedScreen();

  EXPECT_THAT(rendered, HasSubstr("[r:rename]"));
  EXPECT_THAT(rendered, Not(HasSubstr("d:remove")));

  // Use a wider screen
  ftxui::Dimensions wide{.dimx = 180, .dimy = size.dimy};
  screen = std::make_unique<ftxui::Screen>(wide.dimx, wide.dimy);

  ftxui::Render(*screen, dialog->Render(wide));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("[r:rename d:remove]"));

  // After removing the only song, there is nothing else to remove
  dialog->OnEvent(interface::keybinding::Playlist::RemoveSong);

  screen->Clear();
  ftxui::Render(*screen, dialog->Render(wide));
  rendered = GetRenderedScreen();

  EXPECT_THAT(rendered, HasSubstr("[r:rename]"));
  EXPECT_THAT(rendered, Not(HasSubstr("d:remove")));
}

/* ********************************************************************************************** */

/**
 * @brief Tests with ErrorDialog class
 */
class ErrorDialogTest : public ::DialogTest {
 protected:
  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(size.dimx, size.dimy);
    dispatcher = std::make_shared<EventDispatcherMock>();
    error_dialog = std::make_shared<interface::ErrorDialog>(dispatcher);
    dialog = error_dialog;
  }

  //! Getter for ErrorDialog
  auto GetErrorDialog() -> interface::ErrorDialog* { return error_dialog.get(); }

  //! Render dialog and return its content (without ANSI commands and empty spaces)
  std::string Render() {
    screen->Clear();
    ftxui::Render(*screen, dialog->Render(size));
    return utils::FilterEmptySpaces(utils::FilterAnsiCommands(screen->ToString()));
  }

  //!< Screen dimension
  ftxui::Dimensions size = ftxui::Dimensions{.dimx = 50, .dimy = 12};

  std::shared_ptr<interface::ErrorDialog> error_dialog;  //!< Same as dialog, but without casting
};

/* ********************************************************************************************** */

TEST_F(ErrorDialogTest, ShowMessageWithoutDetail) {
  GetErrorDialog()->SetErrorMessage("Cannot decode song", "");

  const std::string expected = R"(
╔═══════════════════════════════════╗
║ ERROR                             ║
║                                   ║
║        Cannot decode song         ║
║                                   ║
║                                   ║
╚═══════════════════════════════════╝
)";

  EXPECT_THAT(Render(), StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ErrorDialogTest, ShowMessageWithDetail) {
  GetErrorDialog()->SetErrorMessage("Cannot decode song", "Daft Punk - Around the World.mp3");

  const std::string expected = R"(
╔═══════════════════════════════════╗
║ ERROR                             ║
║                                   ║
║        Cannot decode song         ║
║                                   ║
║ Daft Punk - Around the World.mp3  ║
║                                   ║
║                                   ║
║                                   ║
╚═══════════════════════════════════╝
)";

  EXPECT_THAT(Render(), StrEq(expected));

  // After closing it, detail must not be shown again for a new error without detail
  dialog->OnEvent(ftxui::Event::Return);
  GetErrorDialog()->SetErrorMessage("File not supported", "");

  EXPECT_THAT(Render(), Not(HasSubstr("Daft Punk")));
}

/* ********************************************************************************************** */

/**
 * @brief Tests with QuestionDialog class
 */
class QuestionDialogTest : public ::DialogTest {
 protected:
  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(size.dimx, size.dimy);
    dispatcher = std::make_shared<EventDispatcherMock>();
    question_dialog = std::make_shared<interface::QuestionDialog>(dispatcher);
    dialog = question_dialog;
  }

  //! Show question dialog with mocked callbacks
  void Ask() {
    question_dialog->SetMessage(model::QuestionData{
        .question = "Do you want to delete \"Chill mix\"?",
        .cb_yes = cb_yes.AsStdFunction(),
        .cb_no = cb_no.AsStdFunction(),
    });

    dialog->Open();
  }

  //!< Screen dimension
  ftxui::Dimensions size = ftxui::Dimensions{.dimx = 60, .dimy = 12};

  std::shared_ptr<interface::QuestionDialog> question_dialog;  //!< Same as dialog, without casting

  MockFunction<void()> cb_yes;  //!< Callback for "Yes" button
  MockFunction<void()> cb_no;   //!< Callback for "No" button
};

/* ********************************************************************************************** */

TEST_F(QuestionDialogTest, ReturnPressesNoByDefault) {
  EXPECT_CALL(cb_yes, Call).Times(0);
  EXPECT_CALL(cb_no, Call);

  Ask();
  dialog->OnEvent(ftxui::Event::Return);

  EXPECT_FALSE(dialog->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(QuestionDialogTest, SelectYesAndPressReturn) {
  EXPECT_CALL(cb_yes, Call);
  EXPECT_CALL(cb_no, Call).Times(0);

  Ask();

  // Move selection back and forth, ending on "Yes"
  dialog->OnEvent(ftxui::Event::ArrowRight);
  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(ftxui::Event::TabReverse);
  dialog->OnEvent(ftxui::Event::Return);

  EXPECT_FALSE(dialog->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(QuestionDialogTest, SelectionIsResetForNewQuestion) {
  EXPECT_CALL(cb_yes, Call).Times(0);
  EXPECT_CALL(cb_no, Call);

  // Select "Yes" but close dialog without answering
  Ask();
  dialog->OnEvent(ftxui::Event::ArrowRight);
  dialog->OnEvent(ftxui::Event::Escape);

  // New question must start with "No" selected again
  Ask();
  dialog->OnEvent(ftxui::Event::Return);
}

/* ********************************************************************************************** */

TEST_F(QuestionDialogTest, AnswerWithKeybinding) {
  EXPECT_CALL(cb_yes, Call);
  EXPECT_CALL(cb_no, Call).Times(0);

  Ask();
  dialog->OnEvent(ftxui::Event::Character('y'));

  EXPECT_FALSE(dialog->IsVisible());
}

/* ********************************************************************************************** */

/**
 * @brief Tests with HelpDialog class
 */
class HelpDialogTest : public ::DialogTest {
 protected:
  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(size.dimx, size.dimy);
    dispatcher = std::make_shared<EventDispatcherMock>();
    help_dialog = std::make_shared<interface::HelpDialog>(dispatcher);
    dialog = help_dialog;
  }

  //! Render dialog and return its content (without ANSI commands and empty spaces)
  std::string Render() {
    screen->Clear();
    ftxui::Render(*screen, dialog->Render(size));
    return utils::FilterEmptySpaces(utils::FilterAnsiCommands(screen->ToString()));
  }

  //! Get first line from help content (just after dialog title)
  std::string GetFirstContentLine() {
    std::istringstream lines{Render()};
    std::string line;

    // Skip lines until reaching dialog title
    while (std::getline(lines, line) && line.find("Help") == std::string::npos) {
    }

    // Then skip empty lines (containing only dialog border)
    auto is_empty = [](std::string text) {
      const std::string border{"║"};
      for (auto pos = text.find(border); pos != std::string::npos; pos = text.find(border)) {
        text.erase(pos, border.size());
      }

      return text.find_first_not_of(' ') == std::string::npos;
    };

    while (std::getline(lines, line) && is_empty(line)) {
    }

    return line;
  }

  //!< Screen dimension
  ftxui::Dimensions size = ftxui::Dimensions{.dimx = 100, .dimy = 20};

  std::shared_ptr<interface::HelpDialog> help_dialog;  //!< Same as dialog, but without casting
};

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, ShowSectionRelatedToFocus) {
  help_dialog->Show(interface::HelpDialog::Section::Equalizer);
  EXPECT_TRUE(dialog->IsVisible());

  // Content starts from the given section
  EXPECT_THAT(GetFirstContentLine(), HasSubstr("equalizer"));
  EXPECT_THAT(Render(), HasSubstr("Cycle presets (picker closed)"));

  // Opening it again from another context starts from the related section
  dialog->OnEvent(ftxui::Event::Escape);
  help_dialog->Show(interface::HelpDialog::Section::Player);

  EXPECT_THAT(GetFirstContentLine(), HasSubstr("player"));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, ScrollContent) {
  help_dialog->Show(interface::HelpDialog::Section::General);
  EXPECT_THAT(GetFirstContentLine(), HasSubstr("general"));
  EXPECT_THAT(Render(), HasSubstr("1-"));

  // Scroll down a single line
  dialog->OnEvent(ftxui::Event::Character('j'));
  EXPECT_THAT(Render(), HasSubstr("2-"));

  // Scrolling up from the top keeps it at the top
  dialog->OnEvent(ftxui::Event::Home);
  dialog->OnEvent(ftxui::Event::ArrowUp);
  EXPECT_THAT(GetFirstContentLine(), HasSubstr("general"));

  // Go to the end, last section must be visible (and scrolling down does not go further)
  dialog->OnEvent(ftxui::Event::End);
  const std::string at_end = Render();
  dialog->OnEvent(ftxui::Event::PageDown);

  EXPECT_THAT(at_end, HasSubstr("confirmation dialog"));
  EXPECT_THAT(Render(), StrEq(at_end));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, ContainsAllKeybindings) {
  help_dialog->Show(interface::HelpDialog::Section::General);

  // Collect all content by scrolling page by page
  constexpr int kMaxPages = 10;  //!< More than enough pages to reach the end of help content

  std::string content;
  for (int page = 0; page < kMaxPages; page++) {
    content += Render();
    dialog->OnEvent(ftxui::Event::PageDown);
  }

  // Some keybindings that were missing in the past
  EXPECT_THAT(content, HasSubstr("Decrease/increase bar width"));
  EXPECT_THAT(content, HasSubstr("Rename playlist"));
  EXPECT_THAT(content, HasSubstr("Remove song from playlist"));
  EXPECT_THAT(content, HasSubstr("Save playlist"));
  EXPECT_THAT(content, HasSubstr("Go to previous/next page"));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, SearchKeybindings) {
  help_dialog->Show(interface::HelpDialog::Section::General);

  // Typed text is used to search (even keys that would scroll or close dialog)
  dialog->OnEvent(ftxui::Event::Character('/'));
  utils::QueueCharacterEvents(*dialog, "Shuffle");

  std::string rendered = Render();
  EXPECT_TRUE(dialog->IsVisible());
  EXPECT_THAT(rendered, HasSubstr("Search:"));
  EXPECT_THAT(rendered, HasSubstr("Toggle shuffle"));
  EXPECT_THAT(rendered, HasSubstr("player"));
  EXPECT_THAT(rendered, Not(HasSubstr("Seek forward")));
  EXPECT_THAT(rendered, Not(HasSubstr("general")));

  // Section title matching the search shows all of its entries
  dialog->OnEvent(ftxui::Event::Escape);
  dialog->OnEvent(ftxui::Event::Character('/'));
  utils::QueueCharacterEvents(*dialog, "equalizer");

  rendered = Render();
  EXPECT_THAT(rendered, HasSubstr("Cycle presets (picker closed)"));
  EXPECT_THAT(rendered, Not(HasSubstr("Toggle shuffle")));

  // Nothing matching
  utils::QueueCharacterEvents(*dialog, "qqq");
  EXPECT_THAT(Render(), HasSubstr("No matches"));
  EXPECT_TRUE(dialog->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, ClearSearchBeforeClosing) {
  help_dialog->Show(interface::HelpDialog::Section::General);

  dialog->OnEvent(ftxui::Event::Character('/'));
  utils::QueueCharacterEvents(*dialog, "volume");

  // Stop typing, content keeps filtered
  dialog->OnEvent(ftxui::Event::Return);
  std::string rendered = Render();
  EXPECT_THAT(rendered, HasSubstr("edit search"));
  EXPECT_THAT(rendered, HasSubstr("Increase/decrease volume"));
  EXPECT_THAT(rendered, Not(HasSubstr("Seek forward")));

  // First escape clears search (showing all content again), second one closes dialog
  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_TRUE(dialog->IsVisible());
  EXPECT_THAT(Render(), Not(HasSubstr("Search:")));
  EXPECT_THAT(GetFirstContentLine(), HasSubstr("general"));

  dialog->OnEvent(ftxui::Event::Escape);
  EXPECT_FALSE(dialog->IsVisible());

  // Opening it again does not keep any previous search
  dialog->OnEvent(ftxui::Event::Character('/'));
  utils::QueueCharacterEvents(*dialog, "volume");
  help_dialog->Show(interface::HelpDialog::Section::Player);

  EXPECT_THAT(Render(), Not(HasSubstr("Search:")));
  EXPECT_THAT(GetFirstContentLine(), HasSubstr("player"));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, ChangeThemeAfterCreation) {
  utils::ThemeGuard guard;

  const auto background = utils::MarkerColor(1);
  const auto file = utils::MarkerColor(2);
  const auto song = utils::MarkerColor(3);
  const auto tab = utils::MarkerColor(4);
  const auto button = utils::MarkerColor(5);
  const auto all = {background, file, song, tab, button};

  GetPlaylistDialog()->Open(model::PlaylistOperation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  });

  // Add first file to playlist
  EXPECT_CALL(contains_audio_cb, Call).WillRepeatedly(Return(true));
  utils::QueueCharacterEvents(*dialog, "j ");

  // Dialog was created with default theme
  ftxui::Render(*screen, dialog->Render(size));
  for (const auto& color : all) EXPECT_FALSE(utils::HasColor(*screen, color));

  // Replace theme, the same dialog must use new colors on next render
  interface::Theme theme;
  theme.dialog.background = background;
  theme.dialog.menu_file = file;
  theme.dialog.menu_song = song;
  theme.dialog.tab = utils::AllButtonStates(tab);
  theme.dialog.button = utils::AllButtonStates(button);
  interface::SetTheme(theme);

  ftxui::Render(*screen, dialog->Render(size));
  for (const auto& color : all) EXPECT_TRUE(utils::HasColor(*screen, color));
}

/* ********************************************************************************************** */

TEST_F(ErrorDialogTest, ChangeThemeAfterCreation) {
  utils::ThemeGuard guard;

  const auto background = utils::MarkerColor(1);

  GetErrorDialog()->SetErrorMessage("Cannot decode song", "");

  // Dialog was created with default theme
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_FALSE(utils::HasColor(*screen, background));

  // Replace theme, the same dialog must use new colors on next render (only the one for errors)
  interface::Theme theme;
  theme.dialog.background_error = background;
  theme.dialog.background = utils::MarkerColor(2);
  interface::SetTheme(theme);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_TRUE(utils::HasColor(*screen, background));
  EXPECT_FALSE(utils::HasColor(*screen, theme.dialog.background));
}

/* ********************************************************************************************** */

TEST_F(QuestionDialogTest, ChangeThemeAfterCreation) {
  utils::ThemeGuard guard;

  const auto background = utils::MarkerColor(1);
  const auto button = utils::MarkerColor(2);

  Ask();

  // Dialog was created with default theme
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_FALSE(utils::HasColor(*screen, background));
  EXPECT_FALSE(utils::HasColor(*screen, button));

  // Replace theme, the same dialog must use new colors on next render
  interface::Theme theme;
  theme.dialog.background = background;
  theme.dialog.answer = utils::AllButtonStates(button);
  interface::SetTheme(theme);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_TRUE(utils::HasColor(*screen, background));
  EXPECT_TRUE(utils::HasColor(*screen, button));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, ChangeThemeAfterCreation) {
  utils::ThemeGuard guard;

  const auto background = utils::MarkerColor(1);

  help_dialog->Show(interface::HelpDialog::Section::Equalizer);

  // Dialog was created with default theme
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_FALSE(utils::HasColor(*screen, background));

  // Replace theme, the same dialog must use new colors on next render
  interface::Theme theme;
  theme.dialog.background = background;
  interface::SetTheme(theme);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_TRUE(utils::HasColor(*screen, background));
}

/* ********************************************************************************************** */

/**
 * @brief Tests with ThemePicker class
 */
class ThemePickerTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(32, 10);
    file_handler = std::make_shared<NiceMock<FileHandlerMock>>();
  }

  //! Create picker, as if the given theme was saved on last run (empty means no theme saved)
  void CreatePicker(const std::string& saved = "") {
    if (!saved.empty()) {
      EXPECT_CALL(*file_handler, ParseSettings(_))
          .WillOnce(DoAll(SetArgReferee<0>(model::Settings{.theme = saved}), Return(true)));
    }

    picker = std::make_unique<interface::ThemePicker>(file_handler);
  }

  //! Check if theme in use is the one with the given identifier
  bool IsThemeInUse(const std::string& id) {
    for (const auto& theme : interface::GetThemes()) {
      if (theme.id != id) continue;

      const auto& current = interface::GetTheme();
      return current.picker.border == theme.colors.picker.border &&
             current.dialog.background == theme.colors.dialog.background;
    }

    return false;
  }

  //! Getter for rendered screen
  std::string GetRenderedScreen() {
    ftxui::Render(*screen, picker->Render());
    return utils::FilterEmptySpaces(utils::FilterAnsiCommands(screen->ToString()));
  }

  utils::ThemeGuard guard;  //!< Restore default theme when test finishes
  std::unique_ptr<ftxui::Screen> screen;
  std::shared_ptr<NiceMock<FileHandlerMock>> file_handler;
  std::unique_ptr<interface::ThemePicker> picker;
};

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, DefaultThemeWithoutSettings) {
  CreatePicker();

  EXPECT_FALSE(picker->IsVisible());
  EXPECT_TRUE(IsThemeInUse("tokyo-night"));

  // Picker does not handle anything while closed
  EXPECT_FALSE(picker->OnEvent(interface::keybinding::Navigation::ArrowDown));
  EXPECT_TRUE(IsThemeInUse("tokyo-night"));
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, RestoreThemeFromSettings) {
  CreatePicker("gruvbox-dark");
  EXPECT_TRUE(IsThemeInUse("gruvbox-dark"));

  // Theme restored is the one selected when picker is opened
  picker->Open();
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ Gruvbox Dark"));
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, UnknownThemeFallsBackToDefault) {
  CreatePicker("does-not-exist");
  EXPECT_TRUE(IsThemeInUse("tokyo-night"));
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, RenderAllThemes) {
  CreatePicker();
  picker->Open();
  EXPECT_TRUE(picker->IsVisible());

  std::string expected = R"(
╭ theme ─────────────╮
│▶ Tokyo Night       │
│  Catppuccin Mocha  │
│  Gruvbox Dark      │
│  Nord              │
│  Dracula           │
╰────────────────────╯
)";

  EXPECT_THAT(GetRenderedScreen(), StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, PreviewAndKeepTheme) {
  using Keybind = interface::keybinding::Navigation;

  CreatePicker();
  picker->Open();

  // Theme is applied while selection moves, but saved only when it is chosen
  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  EXPECT_TRUE(picker->OnEvent(Keybind::ArrowDown));
  EXPECT_TRUE(IsThemeInUse("catppuccin-mocha"));

  EXPECT_TRUE(picker->OnEvent(Keybind::Down));
  EXPECT_TRUE(IsThemeInUse("gruvbox-dark"));

  EXPECT_TRUE(picker->OnEvent(Keybind::Up));
  EXPECT_TRUE(IsThemeInUse("catppuccin-mocha"));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ Catppuccin Mocha"));

  ::testing::Mock::VerifyAndClearExpectations(file_handler.get());

  EXPECT_CALL(*file_handler,
              SaveSettings(Field(&model::Settings::theme, Optional(Eq("catppuccin-mocha")))))
      .WillOnce(Return(true));

  EXPECT_TRUE(picker->OnEvent(Keybind::Return));
  EXPECT_FALSE(picker->IsVisible());
  EXPECT_TRUE(IsThemeInUse("catppuccin-mocha"));
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, CancelRestoresPreviousTheme) {
  using Keybind = interface::keybinding::Navigation;

  CreatePicker("nord");
  picker->Open();

  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  EXPECT_TRUE(picker->OnEvent(Keybind::ArrowUp));
  EXPECT_TRUE(picker->OnEvent(Keybind::ArrowUp));
  EXPECT_TRUE(IsThemeInUse("catppuccin-mocha"));

  EXPECT_TRUE(picker->OnEvent(Keybind::Escape));
  EXPECT_FALSE(picker->IsVisible());
  EXPECT_TRUE(IsThemeInUse("nord"));
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, SelectionStopsAtFirstAndLastTheme) {
  using Keybind = interface::keybinding::Navigation;

  CreatePicker();
  picker->Open();

  EXPECT_TRUE(picker->OnEvent(Keybind::ArrowUp));
  EXPECT_TRUE(IsThemeInUse("tokyo-night"));

  for (size_t i = 0; i < interface::GetThemes().size() + 1; i++) {
    EXPECT_TRUE(picker->OnEvent(Keybind::ArrowDown));
  }

  EXPECT_TRUE(IsThemeInUse(std::string{interface::GetThemes().back().id}));

  // Any other key is not passed along while picker is open
  EXPECT_TRUE(picker->OnEvent(ftxui::Event::Character('p')));
  EXPECT_TRUE(picker->IsVisible());
}

}  // namespace
