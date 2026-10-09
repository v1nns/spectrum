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
    screen = std::make_unique<ftxui::Screen>(96, 6);

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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
│  Phantasy Star Online                                              shuffle off  repeat off   │
│  Deko                                                                  vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   00:00 ──────────────────────────────────────────────────────────── 03:13  │
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
│  After Dark                                                        shuffle off  repeat off   │
│  Mr.Kitty                                                              vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   01:43 ━━━━━━━━━━━━━━━━━━━━━━━●──────────────────────────────────── 04:19  │
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
│  Slow Love                                                         shuffle off  repeat off   │
│  TENDER                                                                vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   00:11 ━━━●──────────────────────────────────────────────────────── 04:12  │
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
│  Slow Love                                                         shuffle off  repeat off   │
│  TENDER                                                                vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   00:12 ━━━●──────────────────────────────────────────────────────── 04:12  │
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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━─  85%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ──────────   0%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
╰──────────────────────────────────────────────────────────────────────────────────────────────╯)";

  EXPECT_THAT(rendered, StrEq(expected));

  // Use toggle volume keybind again
  block->OnEvent(ftxui::Event::Character('m'));

  // Render screen
  ftxui::Render(*screen, block->Render());

  rendered = utils::FilterAnsiCommands(screen->ToString());

  expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
│  Sos                                                               shuffle off  repeat off   │
│  Timothy Fleet                                                         vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   01:43 ━━━━━━━━━━━━━━━━━━━━━━━●──────────────────────────────────── 04:19  │
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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
│  DUST                                                              shuffle off  repeat off   │
│  cln                                                                   vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   01:43 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━●───────────────── 02:26  │
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
│  DUST                                                              shuffle off  repeat off   │
│  cln                                                                   vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   01:43 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━●───────────────── 02:26  │
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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
│  Best for you                                                      shuffle off  repeat off   │
│  Blood Cultures                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   01:23 ━━━━━━━━━━━━━━━━━━━━━━━●──────────────────────────────────── 03:33  │
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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
│  After World                                                       shuffle off  repeat off   │
│  chipbagov                                                             vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   01:23 ━━━━━━━━━━━━━━━━━━━━━━━●──────────────────────────────────── 03:33  │
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
│  atomic                                                            shuffle off  repeat off   │
│  Aziya                                                                 vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   00:01 ●─────────────────────────────────────────────────────────── 03:33  │
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
│  Save This Wrld                                                    shuffle off  repeat off   │
│  Exyl                                                                  vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   01:03 ━━━━━━━━━━━━━━━━━●────────────────────────────────────────── 03:33  │
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
│  Clair                                                             shuffle off  repeat off   │
│  midwxst                                                               vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ∥   ■   ▶▶   00:01 ●─────────────────────────────────────────────────────────── 03:33  │
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
│                                                                    shuffle off  repeat off   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                File not supported: broken.mp3                                │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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

  for (int i = 0; i < 4; ++i) block->OnEvent(ftxui::Event::Character('R'));

  EXPECT_CALL(*dispatcher,
              SendEvent(AllOf(Field(&interface::CustomEvent::id,
                                    interface::CustomEvent::Identifier::SetShuffle),
                              Field(&interface::CustomEvent::content, VariantWith<bool>(true)))));
  block->OnEvent(ftxui::Event::Character('x'));

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  std::string expected = R"(
╭ player ──────────────────────────────────────────────────────────────────────────────────────╮
│                                                                     shuffle on  repeat all   │
│                                                                        vol ━━━━━━━━━━ 100%   │
│                                                                                              │
│   ◀◀  ▶   ■   ▶▶   --:-- ──────────────────────────────────────────────────────────── --:--  │
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
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), HasSubstr("  40% "));

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

TEST_F(MediaPlayerTest, ShowSongWithLongTitleOrWithoutTags) {
  // Not enough space for the whole title, so it is cut with an ellipsis (keeping modes and volume)
  Process(interface::CustomEvent::UpdateSongInfo(model::Song{
      .filepath = "/music/song.mp3",
      .artist = "Some artist with a really long name, that also does not fit in a single line",
      .title = "Some song with a really long title, that does not fit in a single line of player",
  }));

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered,
              HasSubstr("│  Some song with a really long title, that does not fit in a singl… "
                        "shuffle off  repeat off   │"));
  EXPECT_THAT(rendered,
              HasSubstr("│  Some artist with a really long name, that also does not fit in a sin… "
                        "vol ━━━━━━━━━━ 100%   │"));

  // Song without tags is shown by its filename
  Process(interface::CustomEvent::UpdateSongInfo(model::Song{.filepath = "/music/song.mp3"}));

  screen->Clear();
  ftxui::Render(*screen, block->Render());
  rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, HasSubstr("│  song.mp3 "));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, ChangeThemeAfterCreation) {
  utils::ThemeGuard guard;

  const auto play = utils::MarkerColor(1);
  const auto line = utils::MarkerColor(2);

  // Block was created with default theme
  ftxui::Render(*screen, block->Render());
  EXPECT_FALSE(utils::HasColor(*screen, play));
  EXPECT_FALSE(utils::HasColor(*screen, line));

  // Replace theme, the same block must use new colors on next render
  interface::Theme theme;
  theme.player.play = play;
  theme.player.duration = interface::Theme::State{.foreground = line, .background = line};
  interface::SetTheme(theme);

  ftxui::Render(*screen, block->Render());
  EXPECT_TRUE(utils::HasColor(*screen, play));
  EXPECT_TRUE(utils::HasColor(*screen, line));
}

/* ********************************************************************************************** */

/**
 * @brief Tests with mouse/seek events on MediaPlayer (coordinates based on its fixed-size screen)
 */
class MediaPlayerMouseTest : public MediaPlayerTest {
 protected:
  //! Position of media buttons on screen
  static constexpr int kButtonRow = 4;
  static constexpr int kPreviousColumn = 4;
  static constexpr int kPlayColumn = 8;
  static constexpr int kStopColumn = 12;
  static constexpr int kNextColumn = 16;

  //! Position of song duration line on screen
  static constexpr int kDurationRow = 4;
  static constexpr int kDurationFirstColumn = 27;
  static constexpr int kDurationMiddleColumn = 57;
  static constexpr int kDurationLastColumn = 86;

  //! Position of block title on screen
  static constexpr int kTitleRow = 0;
  static constexpr int kTitleColumn = 2;

  //! Position of shuffle and repeat modes on screen
  static constexpr int kModeRow = 1;
  static constexpr int kShuffleColumn = 70;
  static constexpr int kRepeatColumn = 83;

  //! Position of volume on screen (label, line and percentage)
  static constexpr int kVolumeRow = 2;
  static constexpr int kVolumeLabelColumn = 73;
  static constexpr int kVolumeFirstColumn = 77;
  static constexpr int kVolumeLastColumn = 86;
  static constexpr int kVolumePercentageColumn = 88;

  //! Seconds from song for each column from duration line, to have an exact position for each one
  static constexpr int kSecondsPerColumn = 2;

  //! Song duration (in seconds), which is at the last column from duration line
  static constexpr int kSongDuration =
      (kDurationLastColumn - kDurationFirstColumn) * kSecondsPerColumn;

  //! Song position (in seconds), which is at the middle column from duration line
  static constexpr int kSongPosition =
      (kDurationMiddleColumn - kDurationFirstColumn) * kSecondsPerColumn;

  //! Update block with a song in the given state, and render it to calculate elements position
  void SetSongState(model::Song::MediaState state) {
    Process(interface::CustomEvent::UpdateSongInfo(model::Song{
        .filepath = "/another/custom/path/to/music.mp3",
        .duration = kSongDuration,
    }));

    Process(interface::CustomEvent::UpdateSongState(model::Song::CurrentInformation{
        .state = state,
        .position = kSongPosition,
    }));

    RenderBlock();
  }

  //! Render block to calculate position of each element on screen
  void RenderBlock() { ftxui::Render(*screen, block->Render()); }

  //! Simulate mouse cursor moved to the given position (without any button), and render block
  bool Hover(int x, int y) {
    bool handled = SendMouse(x, y, ftxui::Mouse::None, ftxui::Mouse::Pressed);

    screen->Clear();
    RenderBlock();

    return handled;
  }

  //! Simulate a mouse event on the given position
  bool SendMouse(int x, int y, ftxui::Mouse::Button button = ftxui::Mouse::Left,
                 ftxui::Mouse::Motion motion = ftxui::Mouse::Released) {
    ftxui::Mouse mouse{.button = button, .motion = motion, .x = x, .y = y};
    return block->OnEvent(ftxui::Event::Mouse("", mouse));
  }

  //! Get column where knob from duration line is rendered (or -1, if there is none)
  int GetKnobColumn() {
    for (int x = 0; x < screen->dimx(); x++) {
      if (screen->PixelAt(x, kDurationRow).character == "●") return x;
    }

    return -1;
  }

  //! Expect a single event with the given identifier
  void ExpectEvent(interface::CustomEvent::Identifier id) {
    EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id, id)));
  }

  //! Expect a single event with the given identifier and content
  template <typename T>
  void ExpectEvent(interface::CustomEvent::Identifier id, const T& content) {
    EXPECT_CALL(*dispatcher,
                SendEvent(AllOf(Field(&interface::CustomEvent::id, id),
                                Field(&interface::CustomEvent::content, VariantWith<T>(content)))));
  }
};

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, ClickOnButtonsWithoutSong) {
  using Identifier = interface::CustomEvent::Identifier;
  RenderBlock();

  // Without a song, only play button does something: it asks to play the selected file
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  ExpectEvent(Identifier::PlaySong);

  EXPECT_TRUE(SendMouse(kPlayColumn, kButtonRow));
  EXPECT_TRUE(SendMouse(kStopColumn, kButtonRow));
  EXPECT_TRUE(SendMouse(kPreviousColumn, kButtonRow));
  EXPECT_TRUE(SendMouse(kNextColumn, kButtonRow));

  // Neither a click outside of them, nor on the (empty) song duration bar
  EXPECT_FALSE(SendMouse(0, 0));
  EXPECT_FALSE(SendMouse(kDurationMiddleColumn, kDurationRow));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, ClickOnButtonsWhilePlaying) {
  using Identifier = interface::CustomEvent::Identifier;
  SetSongState(model::Song::MediaState::Play);

  // Play button pauses the current song
  ExpectEvent(Identifier::PauseSong);
  EXPECT_TRUE(SendMouse(kPlayColumn, kButtonRow));

  // And resumes it when paused
  SetSongState(model::Song::MediaState::Pause);

  ExpectEvent(Identifier::ResumeSong, true);
  EXPECT_TRUE(SendMouse(kPlayColumn, kButtonRow));

  ExpectEvent(Identifier::SkipToPreviousPlaylistSong);
  EXPECT_TRUE(SendMouse(kPreviousColumn, kButtonRow));

  ExpectEvent(Identifier::SkipToNextPlaylistSong);
  EXPECT_TRUE(SendMouse(kNextColumn, kButtonRow));

  ExpectEvent(Identifier::StopSong);
  EXPECT_TRUE(SendMouse(kStopColumn, kButtonRow));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, ClickOnButtonAsksForFocus) {
  using Identifier = interface::CustomEvent::Identifier;
  SetSongState(model::Song::MediaState::Play);

  // Simulate another block taking focus
  std::static_pointer_cast<interface::Block>(block)->SetFocused(false);

  for (int column : {kPlayColumn, kStopColumn, kPreviousColumn, kNextColumn}) {
    EXPECT_CALL(*dispatcher, SendEvent(_));
    ExpectEvent(Identifier::SetFocused, model::BlockIdentifier::MediaPlayer);

    EXPECT_TRUE(SendMouse(column, kButtonRow));
    testing::Mock::VerifyAndClearExpectations(dispatcher.get());
  }
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, ClickOnShuffleAndRepeat) {
  using Identifier = interface::CustomEvent::Identifier;
  RenderBlock();

  // Each click acts like its key was pressed
  ExpectEvent(Identifier::SetShuffle, true);
  EXPECT_TRUE(SendMouse(kShuffleColumn, kModeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  ExpectEvent(Identifier::SetShuffle, false);
  EXPECT_TRUE(SendMouse(kShuffleColumn, kModeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  for (auto mode : {model::RepeatMode::All, model::RepeatMode::One, model::RepeatMode::Off}) {
    ExpectEvent(Identifier::SetRepeatMode, mode);
    EXPECT_TRUE(SendMouse(kRepeatColumn, kModeRow));
    testing::Mock::VerifyAndClearExpectations(dispatcher.get());
  }

  // Only when mouse button is released on one of them
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);

  EXPECT_FALSE(SendMouse(kShuffleColumn, kModeRow, ftxui::Mouse::Left, ftxui::Mouse::Pressed));
  EXPECT_FALSE(SendMouse(kRepeatColumn, kModeRow, ftxui::Mouse::None));
  EXPECT_FALSE(SendMouse(kShuffleColumn, kModeRow + 1));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // And it asks for focus, when another block is the one focused
  std::static_pointer_cast<interface::Block>(block)->SetFocused(false);

  ExpectEvent(Identifier::SetShuffle, true);
  ExpectEvent(Identifier::SetFocused, model::BlockIdentifier::MediaPlayer);
  EXPECT_TRUE(SendMouse(kShuffleColumn, kModeRow));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, ClickOnVolumeLine) {
  using Identifier = interface::CustomEvent::Identifier;
  RenderBlock();

  // Line is filled up to the column clicked, and volume is saved as it is done when changed by key
  ExpectEvent(Identifier::SetAudioVolume, model::Volume{0.1F});
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(10))));
  EXPECT_TRUE(SendMouse(kVolumeFirstColumn, kVolumeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  ExpectEvent(Identifier::SetAudioVolume, model::Volume{0.5F});
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(50))));
  EXPECT_TRUE(SendMouse(kVolumeFirstColumn + 4, kVolumeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  ExpectEvent(Identifier::SetAudioVolume, model::Volume{1.F});
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(100))));
  EXPECT_TRUE(SendMouse(kVolumeLastColumn, kVolumeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // Empty space right before line is the one for no volume at all (without muting it)
  ExpectEvent(Identifier::SetAudioVolume, model::Volume{0.F});
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(0))));
  EXPECT_TRUE(SendMouse(kVolumeFirstColumn - 1, kVolumeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  ExpectEvent(Identifier::SetAudioVolume, model::Volume{1.F});
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(100))));
  EXPECT_TRUE(SendMouse(kVolumeLastColumn, kVolumeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // Nothing changes with a click on the same level, or on anything else from volume
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);

  EXPECT_TRUE(SendMouse(kVolumeLastColumn, kVolumeRow));
  EXPECT_FALSE(SendMouse(kVolumePercentageColumn, kVolumeRow));
  EXPECT_FALSE(SendMouse(kVolumeFirstColumn, kVolumeRow + 1));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, ClickOnVolumeLabelTogglesMute) {
  using Identifier = interface::CustomEvent::Identifier;
  RenderBlock();

  auto is_muted = [](bool muted) {
    return AllOf(Field(&interface::CustomEvent::id, Identifier::SetAudioVolume),
                 Field(&interface::CustomEvent::content,
                       VariantWith<model::Volume>(
                           testing::Property(&model::Volume::IsMuted, muted))));
  };

  // A click on label acts like the key to mute volume, as line cannot be clicked before its start
  EXPECT_CALL(*dispatcher, SendEvent(is_muted(true)));
  EXPECT_TRUE(SendMouse(kVolumeLabelColumn, kVolumeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  EXPECT_CALL(*dispatcher, SendEvent(is_muted(false)));
  EXPECT_TRUE(SendMouse(kVolumeLabelColumn, kVolumeRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // Only when mouse button is released on it
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  EXPECT_FALSE(SendMouse(kVolumeLabelColumn, kVolumeRow, ftxui::Mouse::Left, ftxui::Mouse::Pressed));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, HoverOnVolumeWhileMuted) {
  using Identifier = interface::CustomEvent::Identifier;
  utils::ThemeGuard guard;

  const auto hovered = utils::MarkerColor(1);

  interface::Theme theme;
  theme.player.duration_focused =
      interface::Theme::State{.foreground = hovered, .background = hovered};
  interface::SetTheme(theme);

  // Mute volume, which is informed by player
  ExpectEvent(Identifier::SetAudioVolume);
  block->OnEvent(ftxui::Event::Character('m'));

  model::Volume muted;
  muted.ToggleMute();
  Process(interface::CustomEvent::UpdateVolume(muted));

  screen->Clear();
  RenderBlock();
  EXPECT_FALSE(utils::HasColor(*screen, hovered));

  // Label is the one hovered, as there is nothing filled on line while volume is muted
  EXPECT_FALSE(Hover(kVolumeLabelColumn, kVolumeRow));
  EXPECT_EQ(screen->PixelAt(kVolumeLabelColumn, kVolumeRow).foreground_color, hovered);
  EXPECT_TRUE(screen->PixelAt(kVolumeLabelColumn, kVolumeRow).bold);
  EXPECT_FALSE(screen->PixelAt(kVolumeLabelColumn, kVolumeRow).dim);
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, MouseWheelOnVolume) {
  using Identifier = interface::CustomEvent::Identifier;
  RenderBlock();

  auto wheel = [this](ftxui::Mouse::Button button, int column, int row = kVolumeRow) {
    return SendMouse(column, row, button, ftxui::Mouse::Pressed);
  };

  // Mouse wheel changes volume by the same step used by its keys, anywhere on volume
  ExpectEvent(Identifier::SetAudioVolume);
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(95))));
  EXPECT_TRUE(wheel(ftxui::Mouse::WheelDown, kVolumeLabelColumn));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  ExpectEvent(Identifier::SetAudioVolume);
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(90))));
  EXPECT_TRUE(wheel(ftxui::Mouse::WheelDown, kVolumePercentageColumn));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  ExpectEvent(Identifier::SetAudioVolume);
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, Optional(95))));
  EXPECT_TRUE(wheel(ftxui::Mouse::WheelUp, kVolumeFirstColumn));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // But not anywhere else
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);

  EXPECT_FALSE(wheel(ftxui::Mouse::WheelDown, kShuffleColumn, kModeRow));
  EXPECT_FALSE(wheel(ftxui::Mouse::WheelDown, kVolumeFirstColumn, kVolumeRow + 1));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, HoverOnModesAndVolume) {
  using Identifier = interface::CustomEvent::Identifier;
  utils::ThemeGuard guard;

  const auto hovered = utils::MarkerColor(1);
  const auto hovered_accent = utils::MarkerColor(2);

  interface::Theme theme;
  theme.player.button_hovered = hovered;
  theme.player.duration_focused =
      interface::Theme::State{.foreground = hovered_accent, .background = hovered_accent};
  interface::SetTheme(theme);

  RenderBlock();
  EXPECT_FALSE(utils::HasColor(*screen, hovered));
  EXPECT_FALSE(utils::HasColor(*screen, hovered_accent));

  // Mode disabled is hovered like a media button
  for (int column : {kShuffleColumn, kRepeatColumn}) {
    EXPECT_FALSE(Hover(column, kModeRow));
    EXPECT_TRUE(utils::HasColor(*screen, hovered));
    EXPECT_FALSE(utils::HasColor(*screen, hovered_accent));
  }

  // While mode enabled and line with volume level are hovered like the line with song duration
  ExpectEvent(Identifier::SetShuffle, true);
  EXPECT_TRUE(SendMouse(kShuffleColumn, kModeRow));

  EXPECT_FALSE(Hover(kShuffleColumn, kModeRow));
  EXPECT_FALSE(utils::HasColor(*screen, hovered));
  EXPECT_TRUE(utils::HasColor(*screen, hovered_accent));

  EXPECT_FALSE(Hover(kShuffleColumn, kModeRow + 2));
  EXPECT_FALSE(utils::HasColor(*screen, hovered_accent));

  for (int column : {kVolumeLabelColumn, kVolumeFirstColumn, kVolumePercentageColumn}) {
    EXPECT_FALSE(Hover(column, kVolumeRow));
    EXPECT_FALSE(utils::HasColor(*screen, hovered));
    EXPECT_TRUE(utils::HasColor(*screen, hovered_accent));
  }

  // Nothing is hovered after mouse leaves them
  EXPECT_FALSE(Hover(0, 0));
  EXPECT_FALSE(utils::HasColor(*screen, hovered));
  EXPECT_FALSE(utils::HasColor(*screen, hovered_accent));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, MouseOnTitle) {
  using Identifier = interface::CustomEvent::Identifier;
  utils::ThemeGuard guard;

  const auto hovered = utils::MarkerColor(1);

  // Title is hovered like the tab selected from other blocks
  interface::Theme theme;
  theme.block.tab.selected = interface::Theme::State{.foreground = hovered, .background = hovered};
  interface::SetTheme(theme);

  RenderBlock();
  EXPECT_FALSE(utils::HasColor(*screen, hovered));

  EXPECT_FALSE(Hover(kTitleColumn, kTitleRow));
  EXPECT_TRUE(utils::HasColor(*screen, hovered));

  EXPECT_FALSE(Hover(kTitleColumn, kTitleRow + 1));
  EXPECT_FALSE(utils::HasColor(*screen, hovered));

  // A click on it asks for focus, when another block is the one focused
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  EXPECT_TRUE(SendMouse(kTitleColumn, kTitleRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  std::static_pointer_cast<interface::Block>(block)->SetFocused(false);

  ExpectEvent(Identifier::SetFocused, model::BlockIdentifier::MediaPlayer);
  EXPECT_TRUE(SendMouse(kTitleColumn, kTitleRow));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, ClickOnDurationBar) {
  using Identifier = interface::CustomEvent::Identifier;
  SetSongState(model::Song::MediaState::Play);

  // Click on the end of bar seeks forward until the end of song, and asks for focus
  ExpectEvent(Identifier::SeekForwardPosition, kSongDuration - kSongPosition);
  ExpectEvent(Identifier::SetFocused, model::BlockIdentifier::MediaPlayer);
  EXPECT_TRUE(SendMouse(kDurationLastColumn, kDurationRow));

  // Click on the beginning of bar seeks backward until the beginning of song
  ExpectEvent(Identifier::SeekBackwardPosition, kSongPosition);
  ExpectEvent(Identifier::SetFocused, model::BlockIdentifier::MediaPlayer);
  EXPECT_TRUE(SendMouse(kDurationFirstColumn, kDurationRow));

  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // Click on the current position does nothing, same as hovering or clicking outside of bar
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);

  EXPECT_TRUE(SendMouse(kDurationMiddleColumn, kDurationRow));
  EXPECT_FALSE(SendMouse(kDurationMiddleColumn, kDurationRow, ftxui::Mouse::None));
  EXPECT_FALSE(SendMouse(kDurationMiddleColumn, kDurationRow + 1));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, DragKnobOnDurationBar) {
  using Identifier = interface::CustomEvent::Identifier;
  SetSongState(model::Song::MediaState::Play);

  ASSERT_EQ(GetKnobColumn(), kDurationMiddleColumn);

  //! Simulate mouse moved to the given position with left button held, and render block
  auto drag_to = [this](int x, int y) {
    bool handled = SendMouse(x, y, ftxui::Mouse::Left, ftxui::Mouse::Pressed);

    screen->Clear();
    RenderBlock();

    return handled;
  };

  // While button is held, knob follows mouse and song position is not changed
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);

  static constexpr int kColumn = kDurationMiddleColumn + 10;

  EXPECT_TRUE(drag_to(kColumn, kDurationRow));
  EXPECT_EQ(GetKnobColumn(), kColumn);

  EXPECT_TRUE(drag_to(kColumn - 20, kDurationRow));
  EXPECT_EQ(GetKnobColumn(), kColumn - 20);

  // Even past both ends of line (which is where media buttons are)
  EXPECT_TRUE(drag_to(kPlayColumn, kDurationRow));
  EXPECT_EQ(GetKnobColumn(), kDurationFirstColumn);

  EXPECT_TRUE(drag_to(kDurationLastColumn + 3, kDurationRow));
  EXPECT_EQ(GetKnobColumn(), kDurationLastColumn);

  // Knob goes back to song position if mouse leaves the line
  EXPECT_FALSE(drag_to(kColumn, kDurationRow - 1));
  EXPECT_EQ(GetKnobColumn(), kDurationMiddleColumn);

  EXPECT_FALSE(SendMouse(kColumn, kDurationRow - 1));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // Song position is changed only once, when button is released (even over a media button,
  // which is not clicked)
  EXPECT_TRUE(drag_to(kColumn, kDurationRow));
  EXPECT_TRUE(drag_to(kPlayColumn, kDurationRow));

  ExpectEvent(Identifier::SeekBackwardPosition, kSongPosition);
  ExpectEvent(Identifier::SetFocused, model::BlockIdentifier::MediaPlayer);

  EXPECT_TRUE(SendMouse(kPlayColumn, kDurationRow));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // Nothing else is picked after that
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  EXPECT_FALSE(SendMouse(kColumn, kDurationRow - 1));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, KeepKnobOnPickedPositionUntilInformedByPlayer) {
  SetSongState(model::Song::MediaState::Play);

  static constexpr int kColumns = 10;
  static constexpr int kColumn = kDurationMiddleColumn + kColumns;
  static constexpr int kOffset = kColumns * kSecondsPerColumn;

  //! Simulate player informing its position, and render block
  auto update_position = [this](int position) {
    Process(interface::CustomEvent::UpdateSongState(model::Song::CurrentInformation{
        .state = model::Song::MediaState::Play,
        .position = static_cast<uint32_t>(position),
    }));

    screen->Clear();
    RenderBlock();
  };

  //! Simulate a click on the given column from duration line, and render block
  auto click_on = [this](int column) {
    EXPECT_CALL(*dispatcher, SendEvent(_)).Times(AnyNumber());
    EXPECT_TRUE(SendMouse(column, kDurationRow));
    testing::Mock::VerifyAndClearExpectations(dispatcher.get());

    screen->Clear();
    RenderBlock();
  };

  // Knob goes to the position picked right away
  click_on(kColumn);
  EXPECT_EQ(GetKnobColumn(), kColumn);

  // And it does not go back to the old one, which may still be informed by player before it
  // changes song position
  update_position(kSongPosition + 1);
  EXPECT_EQ(GetKnobColumn(), kColumn);

  // After that, it follows player again
  update_position(kSongPosition + kOffset);
  EXPECT_EQ(GetKnobColumn(), kColumn);

  update_position(kSongPosition + kOffset + kSecondsPerColumn);
  EXPECT_EQ(GetKnobColumn(), kColumn + 1);

  // Position picked is not shown forever when player never informs it
  click_on(kDurationMiddleColumn);
  EXPECT_EQ(GetKnobColumn(), kDurationMiddleColumn);

  update_position(kSongPosition + kOffset + kSecondsPerColumn);
  EXPECT_EQ(GetKnobColumn(), kDurationMiddleColumn);

  update_position(kSongPosition + kOffset + 2 * kSecondsPerColumn);
  EXPECT_EQ(GetKnobColumn(), kColumn + 2);

  // And not at all for the end of song, as player ignores it
  click_on(kDurationLastColumn);
  EXPECT_EQ(GetKnobColumn(), kColumn + 2);
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, ClickOnDurationBarSeeksOnlyOnce) {
  using Identifier = interface::CustomEvent::Identifier;
  SetSongState(model::Song::MediaState::Play);

  // A quarter of song is at a quarter of duration line
  constexpr int kColumn = kDurationFirstColumn + ((kDurationLastColumn - kDurationFirstColumn) / 4);
  constexpr int kExpectedPosition = (kColumn - kDurationFirstColumn) * kSecondsPerColumn;

  // A real click is a button pressed and then released, and song must be moved a single time
  EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id, Identifier::SetFocused)))
      .Times(testing::AnyNumber());
  ExpectEvent(Identifier::SeekBackwardPosition, kSongPosition - kExpectedPosition);

  EXPECT_TRUE(SendMouse(kColumn, kDurationRow, ftxui::Mouse::Left, ftxui::Mouse::Pressed));
  EXPECT_TRUE(SendMouse(kColumn, kDurationRow, ftxui::Mouse::Left, ftxui::Mouse::Released));

  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // And when song gets to this position, knob is rendered exactly where the click was
  Process(interface::CustomEvent::UpdateSongState(model::Song::CurrentInformation{
      .state = model::Song::MediaState::Play,
      .position = kExpectedPosition,
  }));

  screen->Clear();
  RenderBlock();
  EXPECT_EQ(GetKnobColumn(), kColumn);

  // Same thing for both ends of duration line
  for (const auto& [position, column] :
       {std::pair{0, kDurationFirstColumn}, std::pair{kSongDuration, kDurationLastColumn}}) {
    Process(interface::CustomEvent::UpdateSongState(model::Song::CurrentInformation{
        .state = model::Song::MediaState::Play,
        .position = static_cast<uint32_t>(position),
    }));

    screen->Clear();
    RenderBlock();
    EXPECT_EQ(GetKnobColumn(), column) << "position=" << position;
  }
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerMouseTest, SeekWithKeyboard) {
  using Identifier = interface::CustomEvent::Identifier;

  // Nothing to seek without a song
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  block->OnEvent(ftxui::Event::Character('f'));
  block->OnEvent(ftxui::Event::Character('b'));

  testing::Mock::VerifyAndClearExpectations(dispatcher.get());
  SetSongState(model::Song::MediaState::Play);

  // Both keys change position by the same number of seconds
  constexpr int kSeekSeconds = 5;

  ExpectEvent(Identifier::SeekForwardPosition, kSeekSeconds);
  EXPECT_TRUE(block->OnEvent(ftxui::Event::Character('f')));

  ExpectEvent(Identifier::SeekBackwardPosition, kSeekSeconds);
  EXPECT_TRUE(block->OnEvent(ftxui::Event::Character('b')));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, RemoteCommandsWithoutSong) {
  using Identifier = interface::CustomEvent::Identifier;
  using model::RemoteCommand;

  // Without a song, these commands do nothing
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  for (auto command :
       {RemoteCommand::Stop, RemoteCommand::SkipToPrevious, RemoteCommand::SkipToNext,
        RemoteCommand::SeekForward, RemoteCommand::SeekBackward}) {
    Process(interface::CustomEvent::RunRemoteCommand(command));
  }

  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // While these ones do not depend on it
  {
    InSequence seq;
    for (auto id :
         {Identifier::PlaySong, Identifier::SetAudioVolume, Identifier::SetAudioVolume,
          Identifier::SetAudioVolume, Identifier::SetRepeatMode, Identifier::SetShuffle}) {
      EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id, id)));
    }
  }

  for (auto command :
       {RemoteCommand::PlayOrPause, RemoteCommand::VolumeDown, RemoteCommand::VolumeUp,
        RemoteCommand::Mute, RemoteCommand::ToggleRepeat, RemoteCommand::ToggleShuffle}) {
    Process(interface::CustomEvent::RunRemoteCommand(command));
  }

  ftxui::Render(*screen, block->Render());
  std::string rendered = utils::FilterAnsiCommands(screen->ToString());

  EXPECT_THAT(rendered, AllOf(HasSubstr("shuffle on"), HasSubstr("repeat all")));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, RemoteCommandsWhilePlaying) {
  using Identifier = interface::CustomEvent::Identifier;
  using model::RemoteCommand;

  Process(interface::CustomEvent::UpdateSongInfo(model::Song{.duration = 146}));
  Process(interface::CustomEvent::UpdateSongState(
      model::Song::CurrentInformation{.state = model::Song::MediaState::Play, .position = 10}));

  // Each command sends the same event as its key
  {
    InSequence seq;
    for (auto id : {Identifier::PauseSong, Identifier::SeekForwardPosition,
                    Identifier::SeekBackwardPosition, Identifier::SkipToNextPlaylistSong,
                    Identifier::SkipToPreviousPlaylistSong, Identifier::StopSong}) {
      EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id, id)));
    }
  }

  for (auto command :
       {RemoteCommand::PlayOrPause, RemoteCommand::SeekForward, RemoteCommand::SeekBackward,
        RemoteCommand::SkipToNext, RemoteCommand::SkipToPrevious, RemoteCommand::Stop}) {
    Process(interface::CustomEvent::RunRemoteCommand(command));
  }
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, RemoteCommandsToPlayAndPauseDoNotToggle) {
  using Identifier = interface::CustomEvent::Identifier;
  using model::RemoteCommand;
  using State = model::Song::MediaState;

  const auto set_state = [this](State state) {
    Process(interface::CustomEvent::UpdateSongState(
        model::Song::CurrentInformation{.state = state, .position = 10}));
  };

  const auto expect_event = [this](Identifier id) {
    EXPECT_CALL(*dispatcher, SendEvent(Field(&interface::CustomEvent::id, id)));
  };

  // Without a song: nothing to pause, and play starts the selected song
  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  Process(interface::CustomEvent::RunRemoteCommand(RemoteCommand::Pause));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  expect_event(Identifier::PlaySong);
  Process(interface::CustomEvent::RunRemoteCommand(RemoteCommand::Play));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // While playing: play changes nothing, and pause is sent to player
  Process(interface::CustomEvent::UpdateSongInfo(model::Song{.duration = 146}));
  set_state(State::Play);

  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  Process(interface::CustomEvent::RunRemoteCommand(RemoteCommand::Play));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  expect_event(Identifier::PauseSong);
  Process(interface::CustomEvent::RunRemoteCommand(RemoteCommand::Pause));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  // While paused: pause changes nothing, and play resumes song
  set_state(State::Pause);

  EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
  Process(interface::CustomEvent::RunRemoteCommand(RemoteCommand::Pause));
  testing::Mock::VerifyAndClearExpectations(dispatcher.get());

  expect_event(Identifier::ResumeSong);
  Process(interface::CustomEvent::RunRemoteCommand(RemoteCommand::Play));
}

/* ********************************************************************************************** */

TEST_F(MediaPlayerTest, RemoteCommandsWithValue) {
  using model::RemoteCommand;
  using model::RemoteNumber;
  using model::RepeatMode;

  //! Send command and check the only event that must be sent because of it (if any)
  const auto run = [this](const model::RemoteRequest& request,
                          const std::optional<interface::CustomEvent>& expected) {
    if (expected) {
      EXPECT_CALL(*dispatcher,
                  SendEvent(AllOf(Field(&interface::CustomEvent::id, expected->id),
                                  Field(&interface::CustomEvent::content, expected->content))));
    } else {
      EXPECT_CALL(*dispatcher, SendEvent(_)).Times(0);
    }

    Process(interface::CustomEvent::RunRemoteCommand(request));
    testing::Mock::VerifyAndClearExpectations(dispatcher.get());
  };

  const auto volume_event = [](float level) {
    return interface::CustomEvent::SetAudioVolume(model::Volume{level});
  };

  // Volume is saved, as it is done when changed by key
  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, ::testing::Eq(30))));
  run({RemoteCommand::SetVolume, RemoteNumber{30, false}}, volume_event(0.3F));

  EXPECT_CALL(*file_handler, SaveSettings(Field(&model::Settings::volume, ::testing::Eq(45))));
  run({RemoteCommand::SetVolume, RemoteNumber{15, true}}, volume_event(0.45F));

  // Nothing is sent when it does not change (which is the point of using a value)
  EXPECT_CALL(*file_handler, SaveSettings(_)).Times(0);
  run({RemoteCommand::SetVolume, RemoteNumber{45, false}}, std::nullopt);
  testing::Mock::VerifyAndClearExpectations(file_handler.get());

  // Volume is kept inside its limits
  run({RemoteCommand::SetVolume, RemoteNumber{-70, true}}, volume_event(0.F));
  run({RemoteCommand::SetVolume, RemoteNumber{-5, true}}, std::nullopt);
  run({RemoteCommand::SetVolume, RemoteNumber{250, true}}, volume_event(1.F));

  // Mute is not changed by a new volume
  run(RemoteCommand::Mute, interface::CustomEvent::SetAudioVolume(model::Volume{1.F}));
  run({RemoteCommand::SetVolume, RemoteNumber{60, false}}, volume_event(0.6F));

  ftxui::Render(*screen, block->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()), HasSubstr("   0% "));

  run(RemoteCommand::Mute, volume_event(0.6F));

  // Repeat and shuffle are set (not toggled)
  run({RemoteCommand::ToggleRepeat, RepeatMode::One},
      interface::CustomEvent::SetRepeatMode(RepeatMode::One));
  run({RemoteCommand::ToggleRepeat, RepeatMode::One}, std::nullopt);
  run({RemoteCommand::ToggleRepeat, RepeatMode::Off},
      interface::CustomEvent::SetRepeatMode(RepeatMode::Off));

  run({RemoteCommand::ToggleShuffle, true}, interface::CustomEvent::SetShuffle(true));
  run({RemoteCommand::ToggleShuffle, true}, std::nullopt);

  ftxui::Render(*screen, block->Render());
  EXPECT_THAT(utils::FilterAnsiCommands(screen->ToString()),
              AllOf(HasSubstr("shuffle on"), HasSubstr("repeat off"), HasSubstr("  60% ")));

  // There is nothing to seek without a song
  run({RemoteCommand::Seek, RemoteNumber{30, false}}, std::nullopt);

  Process(interface::CustomEvent::UpdateSongInfo(model::Song{.duration = 146}));
  Process(interface::CustomEvent::UpdateSongState(
      model::Song::CurrentInformation{.state = model::Song::MediaState::Play, .position = 50}));

  // Position is reached by seeking from the current one
  run({RemoteCommand::Seek, RemoteNumber{90, false}},
      interface::CustomEvent::SeekForwardPosition(40));
  run({RemoteCommand::Seek, RemoteNumber{20, false}},
      interface::CustomEvent::SeekBackwardPosition(30));
  run({RemoteCommand::Seek, RemoteNumber{50, false}}, std::nullopt);
  run({RemoteCommand::Seek, RemoteNumber{10, true}},
      interface::CustomEvent::SeekForwardPosition(10));
  run({RemoteCommand::Seek, RemoteNumber{-10, true}},
      interface::CustomEvent::SeekBackwardPosition(10));

  // And it never goes outside of song
  run({RemoteCommand::Seek, RemoteNumber{-300, true}},
      interface::CustomEvent::SeekBackwardPosition(50));
  run({RemoteCommand::Seek, RemoteNumber{9999, false}},
      interface::CustomEvent::SeekForwardPosition(95));

  // Application exits
  run(RemoteCommand::Quit, interface::CustomEvent::Exit());
}

}  // namespace
