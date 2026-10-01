#include <gmock/gmock-matchers.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <filesystem>
#include <memory>
#include <sstream>

#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include "general/dialog.h"
#include "general/utils.h"
#include "gmock/gmock.h"
#include "mock/event_dispatcher_mock.h"
#include "model/playlist.h"
#include "model/playlist_operation.h"
#include "model/question_data.h"
#include "util/file_handler.h"
#include "view/element/error_dialog.h"
#include "view/element/help_dialog.h"
#include "view/element/playlist_dialog.h"
#include "view/element/question_dialog.h"

namespace {

using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::Return;
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
║      ╭ files ───────────────────────╮╭ <unnamed> ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ Chill mix ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ <unnamed> ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ <unnamed> ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ summer hits ─────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ <unnamed> ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ only the best ───────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ Melodic House ───────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ Melodic House ───────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ Melodic House ───────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ Melodic House ───────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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

  dialog->OnEvent(ftxui::Event::ArrowLeftCtrl);

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
║      ╭ files ───────────────────────╮╭ not so Lofi ─────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ <unnamed> ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ <unnamed> ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ onceuponatimetherewasa ──────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ <unnamed> ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ <unnamed> ───────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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
║      ╭ files ───────────────────────╮╭ Chill piano ─────────────────╮      ║
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
║      │  general                     ││                              │      ║
║      │  middleware_media_controller.││                              │      ║
║      │  mock                        ││                              │      ║
║      │  util_argparser.cc           ││                              │      ║
║      │                              ││                              │      ║
║      │                              ││                              │      ║
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

  // Without a song, dialog will not send event to save playlist in JSON file
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(0);

  // Save playlist
  dialog->OnEvent(ftxui::Event::Character('s'));
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
  EXPECT_THAT(content, HasSubstr("Save playlist"));
  EXPECT_THAT(content, HasSubstr("Go to previous/next page"));
}

}  // namespace
