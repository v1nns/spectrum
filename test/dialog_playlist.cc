#include <gmock/gmock-matchers.h>
#include <gtest/gtest-message.h>
#include <gtest/gtest-test-part.h>

#include <memory>

#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include "general/dialog.h"
#include "general/utils.h"
#include "gmock/gmock.h"
#include "mock/event_dispatcher_mock.h"
#include "model/playlist.h"
#include "model/playlist_operation.h"
#include "util/file_handler.h"
#include "view/element/playlist_dialog.h"

namespace {

using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::MockFunction;
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

// TODO: update tests to match new directory
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
║                              │     Save     │                              ║
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
║                              │     Save     │                              ║
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
║                              │     Save     │                              ║
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

  dialog->OnEvent(ftxui::Event::Escape);

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
║                              │     Save     │                              ║
║                              └──────────────┘                              ║
╚════════════════════════════════════════════════════════════════════════════╝
)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

/* TODO: tests to create

- Create playlist operation:
  - send non-empty playlist?

- Modify playlist operation:
  - send empty playlist?
  - remove only song and try to save playlist

*/

}  // namespace
