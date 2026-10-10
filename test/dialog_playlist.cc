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
#include "view/base/custom_event.h"
#include "view/base/keybinding.h"
#include "view/element/device_picker.h"
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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

TEST_F(PlaylistDialogTest, RenameFromFilesMenu) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  };

  GetPlaylistDialog()->Open(operation);

  // Setup expectation for checking audio stream on selected file
  EXPECT_CALL(contains_audio_cb, Call).WillOnce(Return(true));

  // Add a song and, with files menu still focused, rename playlist
  std::string typed{"jjjjj rmix"};
  utils::QueueCharacterEvents(*dialog, typed);
  dialog->OnEvent(ftxui::Event::Return);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_THAT(GetRenderedScreen(), AllOf(HasSubstr("mix"), Not(HasSubstr("<unnamed>"))));

  // Playlist has a name now, so it can be saved
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SavePlaylistsToFile)))
      .Times(1);

  dialog->OnEvent(ftxui::Event::Character('s'));
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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

TEST_F(PlaylistDialogTest, ShowReasonWhenPlaylistCannotBeImported) {
  const std::string playlist_url{"https://www.youtube.com/playlist?list=PLabcdefghijklmnop"};

  // Each reason informed by extraction and what is shown for it
  const std::vector<std::pair<error::Code, std::string>> reasons{
      {error::kStreamBlocked, "✗ Refused by YouTube"},
      {error::kStreamBlockedWithCookies, "✗ Refused by YouTube"},
      {error::kStreamCookiesFailed, "✗ Cannot read cookies"},
      {error::kStreamUnavailable, "✗ Playlist is not available"},
      {error::kStreamTimedOut, "✗ Took too long to import"},
      {error::kStreamFetchFailed, "✗ Cannot import playlist"},
  };

  for (const auto& [code, message] : reasons) {
    auto fetch = [code = code](const std::string&, std::vector<model::Song>&,
                               const std::atomic<bool>*) { return code; };

    dialog = std::make_unique<interface::PlaylistDialog>(
        dispatcher, contains_audio_cb.AsStdFunction(), LISTDIR_PATH, nullptr, fetch);

    // Import finishes in another thread, which asks for a refresh to show its result
    std::promise<void> refreshed;
    std::atomic<bool> notified = false;
    EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                             interface::CustomEvent::Identifier::Refresh)))
        .WillRepeatedly(Invoke([&](const interface::CustomEvent&) {
          if (!notified.exchange(true)) refreshed.set_value();
        }));

    model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Create};
    GetPlaylistDialog()->Open(operation);

    dialog->OnEvent(ftxui::Event::F2);
    utils::QueueCharacterEvents(*dialog, playlist_url);
    dialog->OnEvent(ftxui::Event::Return);

    ASSERT_EQ(refreshed.get_future().wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << message;

    // Refresh is received by dialog as an event (from terminal)
    dialog->OnEvent(ftxui::Event::Custom);

    screen->Clear();
    ftxui::Render(*screen, dialog->Render(size));
    EXPECT_THAT(GetRenderedScreen(), HasSubstr(message));

    // Wait for thread from import before releasing what is used by it
    dialog->Close();
    ::testing::Mock::VerifyAndClearExpectations(dispatcher.get());
  }
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
      .playlist =
          model::Playlist{
              .index = 0, .name = "Lofi", .songs = {model::Song{.filepath = "Love song.mp3"}}},
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
      .playlist =
          model::Playlist{
              .index = 0, .name = "", .songs = {model::Song{.filepath = "Love song.mp3"}}},
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
      .playlist =
          model::Playlist{
              .index = 0, .name = "Lofi", .songs = {model::Song{.filepath = "Love song.mp3"}}},
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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
╔═════════════════════════════════════════════════════════════════════════ X ╗
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
║      │  general/                    ││                              │      ║
║      │  middleware_media_controller…││                              │      ║
║      │  mock/                       ││                              │      ║
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

TEST_F(QuestionDialogTest, ShowButtonsWithLongQuestion) {
  auto render = [this](const std::string& name) {
    question_dialog->SetMessage(model::QuestionData{
        .question = "Do you want to delete \"" + name + "\"?",
        .cb_yes = cb_yes.AsStdFunction(),
        .cb_no = cb_no.AsStdFunction(),
    });

    dialog->Open();

    screen->Clear();
    ftxui::Render(*screen, dialog->Render(size));
    return utils::FilterEmptySpaces(utils::FilterAnsiCommands(screen->ToString()));
  };

  // Question in a single line
  std::string expected = R"(
╔═════════════════════════════════════════════╗
║                                             ║
║     Do you want to delete "Chill mix"?      ║
║                                             ║
║                  Yes    No                  ║
║                                             ║
╚═════════════════════════════════════════════╝
)";

  EXPECT_THAT(render("Chill mix"), StrEq(expected));

  // Dialog gets taller when question needs more lines, so its last words and both buttons are
  // still shown
  const std::string long_name = "A playlist with a really really long name that will need at "
                                "least three lines to be shown inside of the dialog";

  std::string rendered = render(long_name);
  EXPECT_THAT(rendered, HasSubstr("║ dialog\"?"));
  EXPECT_THAT(rendered, HasSubstr("Yes    No"));

  // And it goes back to its usual size for a short question
  EXPECT_THAT(render("Chill mix"), StrEq(expected));
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

TEST_F(HelpDialogTest, ShowFromFirstLine) {
  help_dialog->Show();
  EXPECT_TRUE(dialog->IsVisible());

  // Content starts from the first section
  EXPECT_THAT(GetFirstContentLine(), HasSubstr("general"));
  EXPECT_THAT(Render(), HasSubstr("Show this help"));

  // Opening it again, after scrolling, starts from the first section again
  dialog->OnEvent(ftxui::Event::PageDown);
  EXPECT_THAT(GetFirstContentLine(), Not(HasSubstr("general")));

  dialog->OnEvent(ftxui::Event::Escape);
  help_dialog->Show();

  EXPECT_THAT(GetFirstContentLine(), HasSubstr("general"));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, ScrollContent) {
  help_dialog->Show();
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
  help_dialog->Show();

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

  // Keybindings from pickers have a section of their own
  EXPECT_THAT(content, HasSubstr("theme and animation pickers"));
  EXPECT_THAT(content, HasSubstr("Preview entry"));
  EXPECT_THAT(content, HasSubstr("Keep entry"));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, ShowKeybindingsInTwoColumnsWhenTheyFit) {
  // Find line (from content rendered) containing the given text
  auto find_line = [](const std::string& rendered, const std::string& text) {
    std::istringstream lines{rendered};
    for (std::string line; std::getline(lines, line);) {
      if (line.find(text) != std::string::npos) return line;
    }

    return std::string{};
  };

  // With the default size from tests, there is space for a single column: the section about
  // playlists is not even visible
  help_dialog->Show();

  std::string rendered = Render();
  EXPECT_THAT(rendered, HasSubstr("1-10 of"));
  EXPECT_THAT(rendered, Not(HasSubstr("Show playlists")));

  // In a wider (and taller) terminal, content continues in a second column
  size = ftxui::Dimensions{.dimx = 140, .dimy = 42};
  screen = std::make_unique<ftxui::Screen>(size.dimx, size.dimy);

  rendered = Render();
  EXPECT_THAT(rendered, HasSubstr("1-54 of"));

  // So the first entry from both columns are in the same line
  const std::string line = find_line(rendered, "Show this help");
  EXPECT_THAT(line, HasSubstr("playlists"));

  // And content is scrolled by everything that is visible
  dialog->OnEvent(ftxui::Event::PageDown);
  rendered = Render();

  EXPECT_THAT(rendered, HasSubstr("Toggle shuffle"));
  EXPECT_THAT(rendered, Not(HasSubstr("Show this help")));

  // Dialog still fits in terminal with two columns (its border is rendered in both sides)
  dialog->OnEvent(ftxui::Event::Home);
  const std::string top = find_line(Render(), "╔");
  EXPECT_THAT(top, HasSubstr("╗"));
  EXPECT_LT(ftxui::string_width(top), size.dimx);

  // A terminal that is not wide enough for both columns keeps a single one
  size = ftxui::Dimensions{.dimx = 120, .dimy = 42};
  screen = std::make_unique<ftxui::Screen>(size.dimx, size.dimy);

  rendered = Render();
  EXPECT_THAT(rendered, HasSubstr("1-27 of"));
  EXPECT_THAT(find_line(rendered, "Show this help"), Not(HasSubstr("playlists")));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, SearchKeybindings) {
  help_dialog->Show();

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
  help_dialog->Show();

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
  help_dialog->Show();

  EXPECT_THAT(Render(), Not(HasSubstr("Search:")));
  EXPECT_THAT(GetFirstContentLine(), HasSubstr("general"));
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

  // Add first two files to playlist (selected entry uses colors from cursor, so a second song is
  // needed to check its own color)
  EXPECT_CALL(contains_audio_cb, Call).WillRepeatedly(Return(true));
  utils::QueueCharacterEvents(*dialog, "j j ");

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
  const auto section = utils::MarkerColor(2);

  help_dialog->Show();

  // Dialog was created with default theme
  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_FALSE(utils::HasColor(*screen, background));
  EXPECT_FALSE(utils::HasColor(*screen, section));

  // Replace theme, the same dialog must use new colors on next render
  interface::Theme theme;
  theme.dialog.background = background;
  theme.dialog.section = section;
  interface::SetTheme(theme);

  ftxui::Render(*screen, dialog->Render(size));
  EXPECT_TRUE(utils::HasColor(*screen, background));
  EXPECT_TRUE(utils::HasColor(*screen, section));
}

/* ********************************************************************************************** */

//! Create event for mouse button released at the position where the given text (ASCII only) is
//! rendered on screen
ftxui::Event MouseEventAt(ftxui::Screen& screen, const std::string& text,
                          ftxui::Mouse::Button button) {
  auto matches = [&](int x, int y) {
    for (size_t i = 0; i < text.size(); i++) {
      int column = x + static_cast<int>(i);
      if (column >= screen.dimx() || screen.PixelAt(column, y).character != text.substr(i, 1)) {
        return false;
      }
    }
    return true;
  };

  for (int y = 0; y < screen.dimy(); y++) {
    for (int x = 0; x < screen.dimx(); x++) {
      if (!matches(x, y)) continue;

      return ftxui::Event::Mouse(
          "", ftxui::Mouse{.button = button, .motion = ftxui::Mouse::Released, .x = x, .y = y});
    }
  }

  ADD_FAILURE() << "Text not found on screen: " << text;
  return ftxui::Event::Custom;
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, ShowSaveButtonOnAnyTerminalSize) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  };

  GetPlaylistDialog()->Open(operation);

  // Size from dialog and from its menus are rounded from terminal size, and there was not enough
  // lines for button with some of them (it was shown only with its border, without any text)
  for (int lines = 24; lines <= 60; lines++) {
    ftxui::Dimensions terminal{.dimx = size.dimx, .dimy = lines};
    screen = std::make_unique<ftxui::Screen>(terminal.dimx, terminal.dimy);

    ftxui::Render(*screen, dialog->Render(terminal));
    EXPECT_THAT(GetRenderedScreen(), HasSubstr("│     Save     │")) << "lines=" << lines;
  }
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, CloseWithMouse) {
  model::PlaylistOperation operation{
      .action = model::PlaylistOperation::Operation::Create,
      .playlist = model::Playlist{},
  };

  GetPlaylistDialog()->Open(operation);
  ftxui::Render(*screen, dialog->Render(size));

  // Dialog is not closed by a click on anything else from it, or outside of it
  dialog->OnEvent(MouseEventAt(*screen, "Create Playlist", ftxui::Mouse::Left));
  EXPECT_TRUE(dialog->IsVisible());

  auto outside = ftxui::Event::Mouse(
      "", ftxui::Mouse{.button = ftxui::Mouse::Left, .motion = ftxui::Mouse::Released});

  EXPECT_FALSE(dialog->OnEvent(outside));
  EXPECT_TRUE(dialog->IsVisible());

  // Only by a click on the button from its border
  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, " X ", ftxui::Mouse::Left)));
  EXPECT_FALSE(dialog->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, RenameWithMouse) {
  utils::ThemeGuard guard;
  const auto hovered = utils::MarkerColor(1);

  interface::Theme theme;
  theme.dialog.tab.focused = interface::Theme::State{.foreground = hovered, .background = hovered};
  interface::SetTheme(theme);

  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Lofi",
                                         .songs = {model::Song{.filepath = "Love song.mp3"}},
                                     }};

  GetPlaylistDialog()->Open(operation);

  auto render = [this] {
    screen->Clear();
    ftxui::Render(*screen, dialog->Render(size));
    return GetRenderedScreen();
  };

  render();
  EXPECT_FALSE(utils::HasColor(*screen, hovered));

  // Playlist name is hovered like a tab from the other pane
  auto hover = MouseEventAt(*screen, "Lofi", ftxui::Mouse::None);
  hover.mouse().motion = ftxui::Mouse::Pressed;

  EXPECT_FALSE(dialog->OnEvent(hover));
  render();
  EXPECT_TRUE(utils::HasColor(*screen, hovered));

  // A click on it starts renaming playlist, exactly like its key (even with focus on files)
  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, "Lofi", ftxui::Mouse::Left)));

  EXPECT_THAT(render(), HasSubstr("[Escape:cancel]"));
  EXPECT_FALSE(utils::HasColor(*screen, hovered));

  std::string typed{" beats"};
  utils::QueueCharacterEvents(*dialog, typed);
  dialog->OnEvent(ftxui::Event::Return);

  EXPECT_THAT(render(), HasSubstr("Lofi beats"));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, ClickOnHints) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Mix",
                                         .songs = {model::Song{.filepath = "dance.mp3"},
                                                   model::Song{.filepath = "trance.mp3"}},
                                     }};

  // Use a screen wide enough for both hints
  ftxui::Dimensions wide{.dimx = 180, .dimy = size.dimy};
  screen = std::make_unique<ftxui::Screen>(wide.dimx, wide.dimy);

  auto render = [this, &wide] {
    screen->Clear();
    ftxui::Render(*screen, dialog->Render(wide));
    return GetRenderedScreen();
  };

  GetPlaylistDialog()->Open(operation);
  dialog->OnEvent(ftxui::Event::Tab);
  ASSERT_THAT(render(), HasSubstr("[r:rename d:remove]"));

  // Each hint acts like its key: remove selected song
  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, "d:remove", ftxui::Mouse::Left)));

  std::string rendered = render();
  EXPECT_THAT(rendered, Not(HasSubstr("dance.mp3")));
  EXPECT_THAT(rendered, HasSubstr("trance.mp3"));

  // Rename playlist
  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, "r:rename", ftxui::Mouse::Left)));
  ASSERT_THAT(render(), HasSubstr("[Escape:cancel]"));

  // And cancel it, keeping the name from before
  utils::QueueCharacterEvents(*dialog, "tape");
  ASSERT_THAT(render(), HasSubstr("Mixtape"));

  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, "Escape:cancel", ftxui::Mouse::Left)));

  rendered = render();
  EXPECT_THAT(rendered, HasSubstr("[r:rename d:remove]"));
  EXPECT_THAT(rendered, Not(HasSubstr("Mixtape")));
  EXPECT_TRUE(dialog->IsVisible());

  // Brackets around hints are not part of them
  EXPECT_FALSE(dialog->OnEvent(MouseEventAt(*screen, "[r:rename", ftxui::Mouse::Left)));
  EXPECT_THAT(render(), HasSubstr("[r:rename d:remove]"));
}

/* ********************************************************************************************** */

TEST_F(PlaylistDialogTest, PlaceCursorWithMouse) {
  model::PlaylistOperation operation{.action = model::PlaylistOperation::Operation::Modify,
                                     .playlist = model::Playlist{
                                         .index = 0,
                                         .name = "Lofi",
                                         .songs = {model::Song{.filepath = "Love song.mp3"}},
                                     }};

  auto render = [this] {
    screen->Clear();
    ftxui::Render(*screen, dialog->Render(size));
    return GetRenderedScreen();
  };

  GetPlaylistDialog()->Open(operation);

  // A click on text typed as URL places cursor on the character clicked
  dialog->OnEvent(ftxui::Event::F2);
  utils::QueueCharacterEvents(*dialog, "youtube.com");
  render();

  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, ".com", ftxui::Mouse::Left)));
  utils::QueueCharacterEvents(*dialog, "!");
  EXPECT_THAT(render(), HasSubstr("youtube!.com"));

  // A second click right after the first one is still a click
  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, "be!", ftxui::Mouse::Left)));
  utils::QueueCharacterEvents(*dialog, "?");
  EXPECT_THAT(render(), HasSubstr("youtu?be!.com"));

  // Same for playlist name, while it is renamed
  dialog->OnEvent(ftxui::Event::Tab);
  dialog->OnEvent(interface::keybinding::Playlist::Rename);
  render();

  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, "ofi", ftxui::Mouse::Left)));
  utils::QueueCharacterEvents(*dialog, "-");
  EXPECT_THAT(render(), HasSubstr("L-ofi"));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, PlaceCursorOnSearchWithMouse) {
  help_dialog->Show();

  // Search and stop typing, so keys are used to scroll content again
  dialog->OnEvent(ftxui::Event::Character('/'));
  utils::QueueCharacterEvents(*dialog, "shufle");
  dialog->OnEvent(ftxui::Event::Return);

  std::string rendered = Render();
  ASSERT_THAT(rendered, HasSubstr("No matches"));
  ASSERT_THAT(rendered, HasSubstr("edit search"));

  // A click on text places cursor on the character clicked, and it is edited again
  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, "le", ftxui::Mouse::Left)));
  utils::QueueCharacterEvents(*dialog, "f");

  rendered = Render();
  EXPECT_THAT(rendered, HasSubstr("Search: shuffle"));
  EXPECT_THAT(rendered, HasSubstr("Toggle shuffle"));
  EXPECT_THAT(rendered, Not(HasSubstr("edit search")));
}

/* ********************************************************************************************** */

TEST_F(HelpDialogTest, CloseWithMouse) {
  help_dialog->Show();
  Render();

  // Dialog is not closed by a click on anything else from it, or outside of it
  dialog->OnEvent(MouseEventAt(*screen, "Help", ftxui::Mouse::Left));
  EXPECT_TRUE(dialog->IsVisible());

  auto outside = ftxui::Event::Mouse(
      "", ftxui::Mouse{.button = ftxui::Mouse::Left, .motion = ftxui::Mouse::Released});

  EXPECT_FALSE(dialog->OnEvent(outside));
  EXPECT_TRUE(dialog->IsVisible());

  // Only by a click on the button from its border
  EXPECT_TRUE(dialog->OnEvent(MouseEventAt(*screen, " X ", ftxui::Mouse::Left)));
  EXPECT_FALSE(dialog->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(ErrorDialogTest, DoNotShowButtonToClose) {
  // A click anywhere on this dialog already closes it
  GetErrorDialog()->SetErrorMessage("Cannot decode song", "");
  EXPECT_THAT(Render(), Not(HasSubstr(" X ")));
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

  //! Getter for rendered screen (with picker placed as terminal does)
  std::string GetRenderedScreen() {
    ftxui::Render(*screen, picker->Render() | ftxui::center);
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
│  Catppuccin Latte  │
│  Terminal          │
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

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, MouseWheelPreviewsTheme) {
  CreatePicker();
  picker->Open();
  GetRenderedScreen();

  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  // Selection moves by one theme, no matter which one is under mouse cursor
  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "Nord", ftxui::Mouse::WheelDown)));
  EXPECT_TRUE(IsThemeInUse("catppuccin-mocha"));

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "Nord", ftxui::Mouse::WheelDown)));
  EXPECT_TRUE(IsThemeInUse("gruvbox-dark"));

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "Nord", ftxui::Mouse::WheelUp)));
  EXPECT_TRUE(IsThemeInUse("catppuccin-mocha"));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ Catppuccin Mocha"));

  // Mouse outside picker does not change anything (and it is not passed along either)
  auto outside = ftxui::Event::Mouse(
      "", ftxui::Mouse{.button = ftxui::Mouse::WheelDown, .motion = ftxui::Mouse::Pressed});
  EXPECT_TRUE(picker->OnEvent(outside));
  EXPECT_TRUE(IsThemeInUse("catppuccin-mocha"));
  EXPECT_TRUE(picker->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, MouseClickPreviewsTheme) {
  CreatePicker();
  picker->Open();
  GetRenderedScreen();

  // Theme is applied, but picker stays open and nothing is saved
  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "Nord", ftxui::Mouse::Left)));
  EXPECT_TRUE(IsThemeInUse("nord"));
  EXPECT_TRUE(picker->IsVisible());
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ Nord"));

  // Click on border does not select anything
  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "theme", ftxui::Mouse::Left)));
  EXPECT_TRUE(IsThemeInUse("nord"));
  EXPECT_TRUE(picker->IsVisible());

  // And theme from before opening picker is still restored when it is cancelled
  EXPECT_TRUE(picker->OnEvent(interface::keybinding::Navigation::Escape));
  EXPECT_TRUE(IsThemeInUse("tokyo-night"));
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, MouseClickOutsideCancels) {
  CreatePicker();
  picker->Open();
  GetRenderedScreen();

  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "Nord", ftxui::Mouse::Left)));
  EXPECT_TRUE(IsThemeInUse("nord"));

  // Mouse button pressed outside of picker is not a click yet
  auto outside = ftxui::Event::Mouse(
      "", ftxui::Mouse{.button = ftxui::Mouse::Left, .motion = ftxui::Mouse::Pressed});

  EXPECT_TRUE(picker->OnEvent(outside));
  EXPECT_TRUE(picker->IsVisible());

  // Picker is closed when it is released, exactly like the key to cancel it
  outside.mouse().motion = ftxui::Mouse::Released;

  EXPECT_TRUE(picker->OnEvent(outside));
  EXPECT_FALSE(picker->IsVisible());
  EXPECT_TRUE(IsThemeInUse("tokyo-night"));
}

/* ********************************************************************************************** */

TEST_F(ThemePickerTest, MouseDoubleClickKeepsTheme) {
  CreatePicker();
  picker->Open();
  GetRenderedScreen();

  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::theme, Optional(Eq("dracula")))))
      .WillOnce(Return(true));

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "Dracula", ftxui::Mouse::Left)));
  EXPECT_TRUE(picker->IsVisible());

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "Dracula", ftxui::Mouse::Left)));
  EXPECT_FALSE(picker->IsVisible());
  EXPECT_TRUE(IsThemeInUse("dracula"));
}

/* ********************************************************************************************** */

/**
 * @brief Tests with DevicePicker class
 */
class DevicePickerTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void SetUp() override {
    screen = std::make_unique<ftxui::Screen>(72, 8);
    dispatcher = std::make_shared<EventDispatcherMock>();
    file_handler = std::make_shared<NiceMock<FileHandlerMock>>();
  }

  //! Create picker, as if the given device was saved on last run (empty means no device saved),
  //! and open it with some devices
  void CreatePicker(const std::string& saved = "") {
    if (!saved.empty()) {
      EXPECT_CALL(*file_handler, ParseSettings(_))
          .WillOnce(DoAll(SetArgReferee<0>(model::Settings{.device = saved}), Return(true)));
    }

    picker = std::make_unique<interface::DevicePicker>(dispatcher, file_handler);
    picker->Open(model::AudioDevices{
        {.name = "default", .description = "Default output"},
        {.name = "pulse", .description = "Sound server"},
        {.name = "front:CARD=DAC,DEV=0", .description = "USB Audio"},
    });
  }

  //! Expect the given device to be sent to audio player and saved in settings
  void ExpectDeviceChosen(const std::string& device) {
    using interface::CustomEvent;

    EXPECT_CALL(*dispatcher,
                SendEvent(AllOf(Field(&CustomEvent::id, CustomEvent::Identifier::SetAudioDevice),
                                Field(&CustomEvent::content, VariantWith<std::string>(device)))));
    EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::device, Optional(Eq(device)))))
        .WillOnce(Return(true));
  }

  //! Getter for rendered screen (with picker placed as terminal does)
  std::string GetRenderedScreen() {
    ftxui::Render(*screen, picker->Render() | ftxui::center);
    return utils::FilterEmptySpaces(utils::FilterAnsiCommands(screen->ToString()));
  }

  utils::ThemeGuard guard;  //!< Restore default theme when test finishes
  std::unique_ptr<ftxui::Screen> screen;
  std::shared_ptr<EventDispatcherMock> dispatcher;
  std::shared_ptr<NiceMock<FileHandlerMock>> file_handler;
  std::unique_ptr<interface::DevicePicker> picker;
};

/* ********************************************************************************************** */

TEST_F(DevicePickerTest, RenderDevices) {
  // Nothing is sent to audio player when picker is created (it starts with device from settings)
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);

  CreatePicker("pulse");
  EXPECT_TRUE(picker->IsVisible());

  // Entries wider than screen are cut, keeping their names visible
  std::string expected = R"(
╭ audio output ────────────────────────────────────────────────────────╮
│  automatic             Default device from system (or the first one  │
│  default               Default output                                │
│▶ pulse                 Sound server                                  │
│  front:CARD=DAC,DEV=0  USB Audio                                     │
╰──────────────────────────────────────────────────────────────────────╯
)";

  EXPECT_THAT(GetRenderedScreen(), StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(DevicePickerTest, MouseWheelMovesSelection) {
  CreatePicker();
  GetRenderedScreen();

  // Device is changed only when it is chosen
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "automatic", ftxui::Mouse::WheelDown)));
  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "automatic", ftxui::Mouse::WheelDown)));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ pulse"));

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "automatic", ftxui::Mouse::WheelUp)));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ default"));

  // Selection does not go beyond first and last entries
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "automatic", ftxui::Mouse::WheelDown)));
  }
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ front:CARD=DAC,DEV=0"));

  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "automatic", ftxui::Mouse::WheelUp)));
  }
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ automatic"));

  // Mouse outside picker does not change anything (and it is not passed along either)
  auto outside = ftxui::Event::Mouse(
      "", ftxui::Mouse{.button = ftxui::Mouse::WheelDown, .motion = ftxui::Mouse::Pressed});
  EXPECT_TRUE(picker->OnEvent(outside));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ automatic"));
  EXPECT_TRUE(picker->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(DevicePickerTest, MouseClickSelectsDevice) {
  CreatePicker();
  GetRenderedScreen();

  // Device is selected, but picker stays open and device is not changed
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "Sound server", ftxui::Mouse::Left)));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ pulse"));
  EXPECT_TRUE(picker->IsVisible());

  // Click on border does not select anything
  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "audio output", ftxui::Mouse::Left)));
  EXPECT_THAT(GetRenderedScreen(), HasSubstr("▶ pulse"));
  EXPECT_TRUE(picker->IsVisible());

  EXPECT_TRUE(picker->OnEvent(interface::keybinding::Navigation::Escape));
  EXPECT_FALSE(picker->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(DevicePickerTest, MouseDoubleClickChoosesDevice) {
  CreatePicker();
  GetRenderedScreen();

  ExpectDeviceChosen("front:CARD=DAC,DEV=0");

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "USB Audio", ftxui::Mouse::Left)));
  EXPECT_TRUE(picker->IsVisible());

  EXPECT_TRUE(picker->OnEvent(MouseEventAt(*screen, "USB Audio", ftxui::Mouse::Left)));
  EXPECT_FALSE(picker->IsVisible());
}

/* ********************************************************************************************** */

TEST_F(DevicePickerTest, ChooseDeviceWithKeyboard) {
  using Keybind = interface::keybinding::Navigation;

  CreatePicker("pulse");

  // First entry is the one to not choose any device
  ExpectDeviceChosen("");

  EXPECT_TRUE(picker->OnEvent(Keybind::ArrowUp));
  EXPECT_TRUE(picker->OnEvent(Keybind::Up));
  EXPECT_TRUE(picker->OnEvent(Keybind::Return));
  EXPECT_FALSE(picker->IsVisible());

  // Picker does not handle anything while closed
  EXPECT_FALSE(picker->OnEvent(Keybind::ArrowDown));
}

}  // namespace
