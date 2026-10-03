#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>

#include "general/block.h"
#include "general/utils.h"
#include "mock/event_dispatcher_mock.h"
#include "mock/file_handler_mock.h"
#include "view/block/media_player.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrEq;
using ::testing::VariantWith;

/**
 * @brief Tests with FileInfo class
 */
class MediaPlayerTest : public ::BlockTest {
 protected:
  void SetUp() override {
    // Create a custom screen with fixed size
    screen = std::make_unique<ftxui::Screen>(96, 12);

    // Create mock for event dispatcher
    dispatcher = std::make_shared<EventDispatcherMock>();

    // Create MediaPlayer block (using a mock to not load/save settings from user's home)
    block = ftxui::Make<interface::MediaPlayer>(dispatcher, file_handler);

    // Set this block as focused
    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);
  }

  //! Load/save settings (by default, there are no settings saved)
  std::shared_ptr<NiceMock<FileHandlerMock>> file_handler =
      std::make_shared<NiceMock<FileHandlerMock>>();
};

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, InitialRender) {
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, UpdateSongInfo) {
  model::Song audio{
      .filepath = "/another/custom/path/to/song.mp3",
      .artist = "Deko",
      .title = "Phantasy Star Online",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 193,
  };

  // Process custom event on block
  auto event = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     00:00                                                                          03:13     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, StartPlaying) {
  model::Song audio{
      .filepath = "/another/custom/path/to/music.mp3",
      .artist = "Mr.Kitty",
      .title = "After Dark",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 259,
  };

  // Process custom event on block to update song info
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Play,
      .position = 103,
  };

  // Process custom event on block to update song state
  auto event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     █████████████████████████████████▎                                                       │
│     01:43                                                                          04:19     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, PauseAndResume) {
  model::Song audio{
      .filepath = "/another/custom/path/to/music.mp3",
      .artist = "TENDER",
      .title = "Slow Love",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 252,
  };

  // Process custom event on block to update song info
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Pause,
      .position = 11,
  };

  // Process custom event on block to update song state, pause song
  auto event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  // Process custom event on block to pause song
  Process(event_info);

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ███▋                                                                                     │
│     00:11                                                                          04:12     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  event_info.content = model::Song::CurrentInformation{
      .state = model::Song::MediaState::Play,
      .position = 12,
  };

  // Process custom event on block to resume song
  Process(event_info);

  screen->Clear();
  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ████                                                                                     │
│     00:12                                                                          04:12     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, ChangeVolume) {
  // Setup mock calls to send back an UpdateVolume event to block
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::SetAudioVolume)))
      .WillRepeatedly(Invoke([&](const interface::CustomEvent& event) {
        auto update_vol = interface::CustomEvent::UpdateVolume(event.GetContent<model::Volume>());
        Process(update_vol);
      }));

  // Simulate keyboard events
  block->OnEvent(ftxui::Event::Character('-'));
  block->OnEvent(ftxui::Event::Character('-'));
  block->OnEvent(ftxui::Event::Character('-'));
  block->OnEvent(ftxui::Event::Character('-'));
  block->OnEvent(ftxui::Event::Character('+'));

  // Render screen
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume:  85%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, ToggleVolumeMute) {
  // Setup mock calls to send back an UpdateVolume event to block
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::SetAudioVolume)))
      .WillRepeatedly(Invoke([&](const interface::CustomEvent& event) {
        auto update_vol = interface::CustomEvent::UpdateVolume(event.GetContent<model::Volume>());
        Process(update_vol);
      }));

  // Use toggle volume keybind
  block->OnEvent(ftxui::Event::Character('m'));

  // Render screen
  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume:   0%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Use toggle volume keybind again
  block->OnEvent(ftxui::Event::Character('m'));

  // Render screen
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, StartPlayingAndClear) {
  model::Song audio{
      .filepath = "/another/custom/path/to/music.mp3",
      .artist = "Timothy Fleet",
      .title = "Sos",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 259,
  };

  // Process custom event on block to update song info
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Play,
      .position = 103,
  };

  // Process custom event on block to update song state
  auto event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     █████████████████████████████████▎                                                       │
│     01:43                                                                          04:19     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  screen->Clear();

  // Process custom event to clear song information
  auto event_clear = interface::CustomEvent::ClearSongInfo();
  Process(event_clear);

  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, StartPlayingAndSendKeyboardCommands) {
  model::Song audio{
      .filepath = "/another/custom/path/to/music.mp3",
      .artist = "cln",
      .title = "DUST",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 146,
  };

  // Process custom event on block to update song info
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Play,
      .position = 103,
  };

  // Process custom event on block to update song state
  auto event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ███████████████████████████████████████████████████████████▏                             │
│     01:43                                                                          02:26     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  screen->Clear();

  // Process keyboard event to pause song
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::PauseSong)));
  auto event_pause = ftxui::Event::Character('p');
  block->OnEvent(event_pause);

  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ███████████████████████████████████████████████████████████▏                             │
│     01:43                                                                          02:26     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  screen->Clear();

  // Setup expectation to invoke custom implementation
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::StopSong)))
      .WillRepeatedly(Invoke([&](const interface::CustomEvent& event) {
        // Simulate audio player sending a ClearSongInformation after song stopped
        auto clear_song = interface::CustomEvent::ClearSongInfo();
        Process(clear_song);
      }));

  // Process keyboard event to stop song
  auto event_stop = ftxui::Event::Character('s');
  block->OnEvent(event_stop);

  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, StartPlayingAndStop) {
  model::Song audio{
      .filepath = "/another/custom/path/to/music.mp3",
      .artist = "Blood Cultures",
      .title = "Best for you",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 213,
  };

  // Process custom event on block to update song info
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Play,
      .position = 83,
  };

  // Process custom event on block to update song state
  auto event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ████████████████████████████████▋                                                        │
│     01:23                                                                          03:33     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  screen->Clear();

  // Process keyboard event to stop song
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::StopSong)))
      .WillRepeatedly(Invoke([&](const interface::CustomEvent& event) {
        auto clear_song = interface::CustomEvent::ClearSongInfo();
        Process(clear_song);
      }));

  auto event_stop = ftxui::Event::Character('s');
  block->OnEvent(event_stop);

  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, AttemptToPlay) {
  // Process keyboard event to play song
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::PlaySong)));

  block->OnEvent(ftxui::Event::Character('p'));

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

TEST_F(MediaPlayerTest, AttemptToSkipSong) {
  // Setup expectations
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SkipToPreviousPlaylistSong)))
      .Times(0);

  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SkipToNextPlaylistSong)))
      .Times(0);

  block->OnEvent(ftxui::Event::Character('<'));
  block->OnEvent(ftxui::Event::Character('<'));
  block->OnEvent(ftxui::Event::Character('>'));
  block->OnEvent(ftxui::Event::Character('>'));

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, StartPlayingAndSkipToNext) {
  model::Song audio{
      .filepath = "/another/custom/path/to/music.mp3",
      .artist = "chipbagov",
      .title = "After World",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 213,
  };

  // Process custom event on block to update song info
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Play,
      .position = 83,
  };

  // Process custom event on block to update song state
  auto event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ████████████████████████████████▋                                                        │
│     01:23                                                                          03:33     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Process keyboard event to skip song
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SkipToNextPlaylistSong)));

  auto event_stop = ftxui::Event::Character('>');
  block->OnEvent(event_stop);

  audio.artist = "Aziya";
  audio.title = "atomic";

  // Process custom event on block to update with new song info
  event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  info.position = 1;

  // Process custom event on block to update song state
  event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  screen->Clear();

  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ▎                                                                                        │
│     00:01                                                                          03:33     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, StartPlayingAndSkipToPrevious) {
  model::Song audio{
      .filepath = "/another/custom/path/to/music.mp3",
      .artist = "Exyl",
      .title = "Save This Wrld",
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_rate = 256000,
      .bit_depth = 32,
      .duration = 213,
  };

  // Process custom event on block to update song info
  auto event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  model::Song::CurrentInformation info{
      .state = model::Song::MediaState::Play,
      .position = 63,
  };

  // Process custom event on block to update song state
  auto event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ████████████████████████▊                                                                │
│     01:03                                                                          03:33     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Process keyboard event to skip song
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SkipToPreviousPlaylistSong)));

  auto event_stop = ftxui::Event::Character('<');
  block->OnEvent(event_stop);

  audio.artist = "midwxst";
  audio.title = "Clair";

  // Process custom event on block to update with new song info
  event_update = interface::CustomEvent::UpdateSongInfo(audio);
  Process(event_update);

  info.position = 1;

  // Process custom event on block to update song state
  event_info = interface::CustomEvent::UpdateSongState(info);
  Process(event_info);

  screen->Clear();

  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││ ⣶  ⣶ ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││ ⣿  ⣿ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ▎                                                                                        │
│     00:01                                                                          03:33     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, ShowWarning) {
  // Warning is shown between media buttons and song duration, without changing anything else
  auto event = interface::CustomEvent::ShowWarning("File not supported: broken.mp3");
  Process(event);

  ftxui::Render(*screen, block->Render());

  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: off              │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: off               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                File not supported: broken.mp3                                │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, ChangeRepeatModeAndShuffle) {
  // Repeat mode cycles through all modes, and each one is sent to audio player
  {
    InSequence seq;
    for (auto mode : {model::RepeatMode::All, model::RepeatMode::One, model::RepeatMode::Off,
                      model::RepeatMode::All}) {
      EXPECT_CALL(
          *dispatcher,
          SendEvent(AllOf(
              Field(&interface::CustomEvent::id, interface::CustomEvent::Identifier::SetRepeatMode),
              Field(&interface::CustomEvent::content, VariantWith<model::RepeatMode>(mode)))));
    }
  }

  for (int i = 0; i < 4; ++i) block->OnEvent(ftxui::Event::Character('t'));

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::SetShuffle),
                              Field(&interface::CustomEvent::content, VariantWith<bool>(true)))));
  block->OnEvent(ftxui::Event::Character('x'));

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                                              │
│                               ╭──────╮╭──────╮╭──────╮╭──────╮                               │
│                               │ ⣶ ⣠⡆ ││  ⣦⡀  ││ ⣶⣶⣶⣶ ││ ⢰⣄ ⣶ │                               │
│                               │ ⣿⢾⣿⡇ ││  ⣿⣿⠆ ││ ⣿⣿⣿⣿ ││ ⢸⣿⡷⣿ │                               │
│     Shuffle: on               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│     Repeat: all               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, RestoreAndSaveVolume) {
  // Volume from last run is restored, and sent to audio player
  EXPECT_CALL(*file_handler, ParseSettings(_))
      .WillOnce(DoAll(SetArgReferee<0>(model::Settings{.volume = 40}), Return(true)));

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::SetAudioVolume),
                              Field(&interface::CustomEvent::content,
                                    VariantWith<model::Volume>(model::Volume{0.4F})))));

  auto restored = ftxui::Make<interface::MediaPlayer>(dispatcher, file_handler);
  std::static_pointer_cast<interface::Block>(restored)->SetFocused(true);

  ftxui::Render(*screen, restored->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), HasSubstr("Volume:  40%"));

  // Changing volume saves it
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(45))))
      .WillOnce(Return(true));
  restored->OnEvent(ftxui::Event::Character('+'));

  // But mute state is not saved
  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);
  restored->OnEvent(ftxui::Event::Character('m'));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, ChangeThemeAfterCreation) {
  utils::ThemeGuard guard;

  const auto play = utils::MarkerColor(1);
  const auto border = utils::MarkerColor(2);

  // Block was created with default theme
  ftxui::Render(*screen, block->Render());
  EXPECT_FALSE(utils::HasColor(*screen, play));
  EXPECT_FALSE(utils::HasColor(*screen, border));

  // Replace theme, the same block must use new colors on next render
  interface::Theme theme;
  theme.player.play = play;
  theme.player.button_border = border;
  interface::SetTheme(theme);

  ftxui::Render(*screen, block->Render());
  EXPECT_TRUE(utils::HasColor(*screen, play));
  EXPECT_TRUE(utils::HasColor(*screen, border));
}

}  // namespace
