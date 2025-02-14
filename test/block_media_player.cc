#include <gmock/gmock-matchers.h>

#include "general/block.h"
#include "general/utils.h"
#include "mock/event_dispatcher_mock.h"
#include "view/block/media_player.h"

namespace {

using ::testing::Field;
using ::testing::Invoke;
using ::testing::StrEq;

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

    // Create MediaPlayer block
    block = ftxui::Make<interface::MediaPlayer>(dispatcher);

    // Set this block as focused
    auto dummy = std::static_pointer_cast<interface::Block>(block);
    dummy->SetFocused(true);
  }
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume:  85%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume:   0%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│                                                                                              │
│     --:--                                                                          --:--     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

TEST_F(MediaPlayerTest, AttemptToSkipSong) {
  // Setup expectations
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::SkipToPreviousSong)))
      .Times(0);

  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::SkipToNextSong)))
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
│                               │ ⠿ ⠙⠇ ││  ⠟⠁  ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ████████████████████████████████▋                                                        │
│     01:23                                                                          03:33     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Process keyboard event to skip song
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id,
                                           interface::CustomEvent::Identifier::SkipToNextSong)));

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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ████████████████████████▊                                                                │
│     01:03                                                                          03:33     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Process keyboard event to skip song
  EXPECT_CALL(*dispatcher,
              SendEvent(Field(&interface::CustomEvent::id,
                              interface::CustomEvent::Identifier::SkipToPreviousSong)));

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
│                               │ ⠿ ⠙⠇ ││ ⠿  ⠿ ││ ⠿⠿⠿⠿ ││ ⠸⠋ ⠿ │                               │
│                               ╰──────╯╰──────╯╰──────╯╰──────╯              Volume: 100%     │
│                                                                                              │
│     ▎                                                                                        │
│     00:01                                                                          03:33     │
│                                                                                              │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));
}

}  // namespace
