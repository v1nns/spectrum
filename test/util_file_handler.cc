#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "general/utils.h"
#include "middleware/remote_client.h"
#include "model/listening_stats.h"
#include "model/mpris.h"
#include "model/player_status.h"
#include "model/playlist.h"
#include "model/remote_command.h"
#include "model/settings.h"
#include "model/song.h"
#include "model/stream_info.h"
#include "nlohmann/json.hpp"
#include "util/file_handler.h"
#include "util/loop_waker.h"
#include "util/process.h"
#include "util/remote.h"
#include "util/sink.h"

namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StrEq;

/**
 * @brief Tests with FileHandler class (using a temporary HOME directory for playlists file)
 */
class FileHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (const char* home = std::getenv("HOME"); home) original_home = home;
    if (const char* config = std::getenv("XDG_CONFIG_HOME"); config) original_config = config;
    if (const char* state = std::getenv("XDG_STATE_HOME"); state) original_state = state;

    home_dir = std::filesystem::temp_directory_path() / "spectrum_file_handler_test";
    std::filesystem::remove_all(home_dir);
    std::filesystem::create_directories(home_dir / ".config" / "spectrum");
    setenv("HOME", home_dir.c_str(), 1);

    // Otherwise files would be saved in the real directory from user
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_STATE_HOME");
  }

  void TearDown() override {
    if (original_home.has_value()) {
      setenv("HOME", original_home->c_str(), 1);
    } else {
      unsetenv("HOME");
    }

    if (original_config.has_value()) {
      setenv("XDG_CONFIG_HOME", original_config->c_str(), 1);
    } else {
      unsetenv("XDG_CONFIG_HOME");
    }

    if (original_state.has_value()) {
      setenv("XDG_STATE_HOME", original_state->c_str(), 1);
    } else {
      unsetenv("XDG_STATE_HOME");
    }

    std::filesystem::remove_all(home_dir);
  }

  //! Write content to file in directory used by older versions
  void WriteLegacyFile(const std::string& filename, const std::string& content) const {
    std::filesystem::create_directories(GetLegacyDirectory());
    std::ofstream(GetLegacyDirectory() / filename) << content;
  }

  //! Get directory used by older versions to save playlists and settings
  std::filesystem::path GetLegacyDirectory() const { return home_dir / ".cache" / "spectrum"; }

  //! Read content from file (empty if it does not exist)
  static std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), {});
  }

  //! Write content to playlists file
  void WritePlaylistsFile(const std::string& content) const {
    std::ofstream out(handler.GetPlaylistsPath());
    out << content;
  }

  //! Read content from backup of playlists file (empty if it does not exist)
  std::string ReadBackupFile() const {
    std::ifstream in(handler.GetPlaylistsPath() + ".bak");
    return std::string(std::istreambuf_iterator<char>(in), {});
  }

  std::optional<std::string> original_home;    //!< HOME before test
  std::optional<std::string> original_config;  //!< XDG_CONFIG_HOME before test
  std::optional<std::string> original_state;   //!< XDG_STATE_HOME before test
  std::filesystem::path home_dir;              //!< Temporary HOME directory
  util::FileHandler handler;                   //!< Class under test
};

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ParseMalformedPlaylistsFile) {
  // File truncated in the middle of it
  const std::string content = R"({"playlists": [)";
  WritePlaylistsFile(content);

  model::Playlists playlists;
  EXPECT_FALSE(handler.ParsePlaylists(playlists));
  EXPECT_THAT(playlists, IsEmpty());

  // Keep a backup, as file will be overwritten on next save
  EXPECT_THAT(ReadBackupFile(), StrEq(content));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ParsePlaylistsFileWithUnexpectedStructure) {
  model::Playlists playlists;

  WritePlaylistsFile(R"([1, 2, 3])");
  EXPECT_FALSE(handler.ParsePlaylists(playlists));

  WritePlaylistsFile(R"({"playlists": 1})");
  EXPECT_FALSE(handler.ParsePlaylists(playlists));

  EXPECT_THAT(playlists, IsEmpty());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SkipOnlyInvalidEntries) {
  // Create an existent file to be used as song
  auto song_path = home_dir / "song.mp3";
  utils::CreateEmptyFile(song_path);

  WritePlaylistsFile(R"({"playlists": [
    {"name": 1, "songs": []},
    "not a playlist",
    {"name": "Wrong songs", "songs": "not a list"},
    {"name": "Good", "songs": [
      {"path": ")" + song_path.string() +
                     R"("},
      {"path": 7},
      {"url": 5},
      {"url": "https://youtu.be/aaaaaaaaaaa", "title": 3},
      {"url": "https://youtu.be/dQw4w9WgXcQ", "title": "Never gonna"}
    ]}
  ]})");

  model::Playlists playlists;
  ASSERT_TRUE(handler.ParsePlaylists(playlists));

  ASSERT_THAT(playlists, SizeIs(1));
  EXPECT_THAT(playlists[0].name, StrEq("Good"));

  // Skipped entries would be lost on next save, so keep a backup
  EXPECT_THAT(ReadBackupFile(), Not(IsEmpty()));

  ASSERT_THAT(playlists[0].songs, SizeIs(2));
  EXPECT_THAT(playlists[0].songs[0].filepath, Eq(song_path));
  ASSERT_TRUE(playlists[0].songs[1].stream_info.has_value());
  EXPECT_THAT(playlists[0].songs[1].stream_info->base_url, StrEq("https://youtu.be/dQw4w9WgXcQ"));
  EXPECT_THAT(playlists[0].songs[1].title, StrEq("Never gonna"));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, NoBackupForValidFile) {
  WritePlaylistsFile(
      R"({"playlists": [{"name": "Good", "songs": [{"url": "https://youtu.be/dQw4w9WgXcQ"}]}]})");

  model::Playlists playlists;
  ASSERT_TRUE(handler.ParsePlaylists(playlists));
  EXPECT_THAT(playlists, SizeIs(1));

  EXPECT_FALSE(std::filesystem::exists(handler.GetPlaylistsPath() + ".bak"));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SaveAndParseSettings) {
  // Nothing saved yet
  model::Settings settings;
  EXPECT_FALSE(handler.ParseSettings(settings));

  ASSERT_TRUE(handler.SaveSettings(
      model::Settings{.animation = model::BarAnimation::SpectrumLine, .bar_width = 3}));

  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.animation, model::BarAnimation::SpectrumLine);
  EXPECT_EQ(settings.bar_width, 3);
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SaveAndParsePlayerModesAndEqualizer) {
  const std::vector<double> gains{3, 0, -2, 0, 0, 12, 0, -12, 0, 1.5};

  ASSERT_TRUE(handler.SaveSettings(
      model::Settings{.repeat = model::RepeatMode::One, .shuffle = true}));
  ASSERT_TRUE(handler.SaveSettings(
      model::Settings{.equalizer_preset = "Rock", .equalizer_custom = gains}));

  model::Settings settings;
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.repeat, model::RepeatMode::One);
  EXPECT_EQ(settings.shuffle, true);
  EXPECT_EQ(settings.equalizer_preset, "Rock");
  EXPECT_EQ(settings.equalizer_custom, gains);

  // They are saved in a way that may be read (and changed) by user
  std::ifstream file(handler.GetSettingsPath());
  const auto json = nlohmann::json::parse(file);

  EXPECT_EQ(json["player"]["repeat"], "one");
  EXPECT_EQ(json["player"]["shuffle"], true);
  EXPECT_EQ(json["equalizer"]["preset"], "Rock");
  EXPECT_EQ(json["equalizer"]["custom"].size(), gains.size());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ParseInvalidPlayerModesAndEqualizer) {
  std::filesystem::create_directories(
      std::filesystem::path{handler.GetSettingsPath()}.parent_path());

  auto parse = [this](const std::string& content) {
    std::ofstream(handler.GetSettingsPath()) << content;

    model::Settings settings;
    EXPECT_TRUE(handler.ParseSettings(settings));
    return settings;
  };

  // Unknown repeat mode and values with unexpected type are not filled
  auto settings = parse(R"({"player": {"repeat": "twice", "shuffle": "on"}})");
  EXPECT_FALSE(settings.repeat.has_value());
  EXPECT_FALSE(settings.shuffle.has_value());

  settings = parse(R"({"equalizer": {"preset": 3, "custom": "flat"}})");
  EXPECT_FALSE(settings.equalizer_preset.has_value());
  EXPECT_FALSE(settings.equalizer_custom.has_value());

  // There must be a gain for each frequency
  settings = parse(R"({"equalizer": {"custom": [1, 2, 3]}})");
  EXPECT_FALSE(settings.equalizer_custom.has_value());

  // And all of them must be numbers inside the limits for gain
  settings = parse(R"({"equalizer": {"custom": [0, 0, 0, 0, 0, 0, 0, 0, 0, 13]}})");
  EXPECT_FALSE(settings.equalizer_custom.has_value());

  settings = parse(R"({"equalizer": {"custom": [0, 0, 0, 0, 0, 0, 0, 0, 0, "max"]}})");
  EXPECT_FALSE(settings.equalizer_custom.has_value());

  settings = parse(R"({"equalizer": {"custom": [0, 0, 0, 0, 0, 0, 0, 0, 0, -12]}})");
  EXPECT_TRUE(settings.equalizer_custom.has_value());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SaveAnimationByItsIdentifier) {
  ASSERT_TRUE(
      handler.SaveSettings(model::Settings{.animation = model::BarAnimation::SpectrumLineFilled}));

  {
    std::ifstream file(handler.GetSettingsPath());
    EXPECT_EQ(nlohmann::json::parse(file)["visualizer"]["animation"], "line-filled");
  }

  // Every animation has its own identifier
  for (int value = model::BarAnimation::HorizontalMirror; value < model::BarAnimation::LAST;
       value++) {
    const auto animation = static_cast<model::BarAnimation>(value);
    EXPECT_EQ(model::GetAnimationFromId(model::GetAnimationId(animation)), animation);
  }

  EXPECT_FALSE(model::GetAnimationFromId("invalid").has_value());

  // Unknown identifier is not filled
  std::ofstream(handler.GetSettingsPath()) << R"({"visualizer": {"animation": "spiral"}})";

  model::Settings settings;
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_FALSE(settings.animation.has_value());

  // Animation saved as a number (by older versions) is still restored
  std::ofstream(handler.GetSettingsPath()) << R"({"visualizer": {"animation": 11002}})";

  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.animation, model::BarAnimation::Mono);
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ParseInvalidSettings) {
  std::filesystem::create_directories(
      std::filesystem::path{handler.GetSettingsPath()}.parent_path());

  // Unknown animation and value with unexpected type are not filled
  std::ofstream(handler.GetSettingsPath())
      << R"({"visualizer": {"animation": 12345, "bar_width": "wide"}})";

  model::Settings settings;
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_FALSE(settings.animation.has_value());
  EXPECT_FALSE(settings.bar_width.has_value());

  // Malformed file is kept as backup
  std::ofstream(handler.GetSettingsPath()) << R"({"visualizer": )";
  EXPECT_FALSE(handler.ParseSettings(settings));
  EXPECT_TRUE(std::filesystem::exists(handler.GetSettingsPath() + ".bak"));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SaveSettingsKeepsOtherSettings) {
  // Visualizer and volume are saved separately (by different blocks)
  ASSERT_TRUE(handler.SaveSettings(
      model::Settings{.animation = model::BarAnimation::Mono, .bar_width = 1}));
  ASSERT_TRUE(handler.SaveSettings(model::Settings{.volume = 35}));

  model::Settings settings;
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.animation, model::BarAnimation::Mono);
  EXPECT_EQ(settings.bar_width, 1);
  EXPECT_EQ(settings.volume, 35);

  // Volume out of range is not filled
  ASSERT_TRUE(handler.SaveSettings(model::Settings{.volume = 150}));
  settings = model::Settings{};
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_FALSE(settings.volume.has_value());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SaveAndParseTheme) {
  // Theme is saved without changing the other settings
  ASSERT_TRUE(handler.SaveSettings(model::Settings{.volume = 35}));
  ASSERT_TRUE(handler.SaveSettings(model::Settings{.theme = "tokyo-night"}));

  model::Settings settings;
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.theme, "tokyo-night");
  EXPECT_EQ(settings.volume, 35);

  // Value with unexpected type is not filled
  std::ofstream(handler.GetSettingsPath()) << R"({"interface": {"theme": 3}})";
  settings = model::Settings{};
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_FALSE(settings.theme.has_value());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SaveAndParseDevice) {
  // Device is saved without changing the other settings
  ASSERT_TRUE(handler.SaveSettings(model::Settings{.volume = 35}));
  ASSERT_TRUE(handler.SaveSettings(model::Settings{.device = "front:CARD=DAC,DEV=0"}));

  model::Settings settings;
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.device, "front:CARD=DAC,DEV=0");
  EXPECT_EQ(settings.volume, 35);

  // Empty value is also saved, as it means that no device is chosen anymore
  ASSERT_TRUE(handler.SaveSettings(model::Settings{.device = ""}));
  settings = model::Settings{};
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.device, "");

  // Value with unexpected type is not filled
  std::ofstream(handler.GetSettingsPath()) << R"({"player": {"device": 3}})";
  settings = model::Settings{};
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_FALSE(settings.device.has_value());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ParseCookiesFromBrowser) {
  std::filesystem::create_directories(
      std::filesystem::path{handler.GetSettingsPath()}.parent_path());

  // It is written by user, as there is nothing in the interface to change it
  std::ofstream(handler.GetSettingsPath())
      << R"({"stream": {"cookies_from_browser": "firefox:music"}})";

  model::Settings settings;
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.cookies_from_browser, "firefox:music");

  // And it is kept when any other setting is saved
  ASSERT_TRUE(handler.SaveSettings(model::Settings{.volume = 35}));
  settings = model::Settings{};
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_EQ(settings.cookies_from_browser, "firefox:music");
  EXPECT_EQ(settings.volume, 35);

  // Value with unexpected type is not filled
  std::ofstream(handler.GetSettingsPath()) << R"({"stream": {"cookies_from_browser": true}})";
  settings = model::Settings{};
  ASSERT_TRUE(handler.ParseSettings(settings));
  EXPECT_FALSE(settings.cookies_from_browser.has_value());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SaveAndParseStats) {
  model::ListeningStats stats{
      .since = 1792982772,
      .listened_ms = 300000,
      .local_ms = 120000,
      .stream_ms = 180000,
      .songs_played = 4,
      .songs_finished = 3,
      .songs_skipped = 1,
      .stream_bytes = 5840670,
      .songs = {{"/music/song.mp3",
                 {.artist = "Artist", .title = "Song", .listened_ms = 120000, .plays = 1}},
                {"https://www.youtube.com/watch?v=id",
                 {.title = "Mix", .listened_ms = 180000, .plays = 3}}},
      .streak = {.day = 20759, .current = 3, .longest = 12},
  };

  stats.hours[0][22] = 120000;
  stats.hours[6][9] = 180000;

  // Nothing saved yet
  model::ListeningStats parsed;
  EXPECT_FALSE(handler.ParseStats(parsed));

  // It is not something chosen by user, so it is kept apart from settings
  ASSERT_TRUE(handler.SaveStats(stats));
  EXPECT_EQ(handler.GetStatsPath(),
            (home_dir / ".local" / "state" / "spectrum" / "stats.json").string());
  EXPECT_TRUE(std::filesystem::exists(handler.GetStatsPath()));
  EXPECT_FALSE(std::filesystem::exists(handler.GetStatsPath() + ".tmp"));

  ASSERT_TRUE(handler.ParseStats(parsed));
  EXPECT_EQ(parsed, stats);

  // Saving again replaces everything saved before
  stats.songs.erase("/music/song.mp3");
  ASSERT_TRUE(handler.SaveStats(stats));
  ASSERT_TRUE(handler.ParseStats(parsed));
  EXPECT_EQ(parsed, stats);
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, StatsPathFromXdgStateHome) {
  setenv("XDG_STATE_HOME", (home_dir / "state").c_str(), 1);
  EXPECT_EQ(handler.GetStatsPath(), (home_dir / "state" / "spectrum" / "stats.json").string());

  // Relative path is ignored (as stated by XDG specification)
  setenv("XDG_STATE_HOME", "state", 1);
  EXPECT_EQ(handler.GetStatsPath(),
            (home_dir / ".local" / "state" / "spectrum" / "stats.json").string());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ParseInvalidStats) {
  std::filesystem::create_directories(std::filesystem::path{handler.GetStatsPath()}.parent_path());

  // Values with unexpected type are not filled, and songs without identifier are skipped
  std::ofstream(handler.GetStatsPath()) << R"({
    "listened_ms": "a lot", "songs_played": 3, "stream_bytes": -1,
    "songs": [{"title": "No key"}, {"key": "/song.mp3", "plays": "2", "listened_ms": 5}, 42]
  })";

  model::ListeningStats stats;
  ASSERT_TRUE(handler.ParseStats(stats));
  EXPECT_EQ(stats.listened_ms, 0);
  EXPECT_EQ(stats.songs_played, 3);
  EXPECT_EQ(stats.stream_bytes, 0);
  ASSERT_THAT(stats.songs, SizeIs(1));
  EXPECT_EQ(stats.songs["/song.mp3"].plays, 0);
  EXPECT_EQ(stats.songs["/song.mp3"].listened_ms, 5);

  // Hours without a list for each weekday, with a value for each hour, are not filled at all
  std::ofstream(handler.GetStatsPath()) << R"({"hours": [[1, 2, 3]], "streak": [1, 2]})";
  ASSERT_TRUE(handler.ParseStats(stats));
  EXPECT_EQ(stats.hours, model::ListeningStats::Hours{});
  EXPECT_EQ(stats.streak, model::ListeningStats::Streak{});

  // While a single bad value is ignored
  nlohmann::json hours(model::ListeningStats::kWeekdays,
                       nlohmann::json(model::ListeningStats::kDayHours, 10));
  hours[1][2] = "a lot";
  hours[3][4] = -5;

  std::ofstream(handler.GetStatsPath())
      << nlohmann::json{{"hours", hours}, {"streak", {{"day", 20759}, {"current", "3"}}}};
  ASSERT_TRUE(handler.ParseStats(stats));
  EXPECT_EQ(stats.hours[0][0], 10);
  EXPECT_EQ(stats.hours[1][2], 0);
  EXPECT_EQ(stats.hours[3][4], 0);
  EXPECT_EQ(stats.hours[6][23], 10);
  EXPECT_EQ(stats.streak.day, 20759);
  EXPECT_EQ(stats.streak.current, 0);

  // Malformed file is kept as backup
  std::ofstream(handler.GetStatsPath()) << R"({"songs": )";
  EXPECT_FALSE(handler.ParseStats(stats));
  EXPECT_TRUE(std::filesystem::exists(handler.GetStatsPath() + ".bak"));
}

/* ********************************************************************************************** */

TEST(ListeningStatsTest, CountListenedAndPlayed) {
  model::ListeningStats stats;
  EXPECT_TRUE(stats.IsEmpty());

  // First song listened to tells since when stats are counted
  stats.AddListened("/song.mp3", "Artist", "Song", false, 1500, 1000);
  stats.AddListened("https://site/mix", "", "Mix", true, 2500, 2000);
  EXPECT_FALSE(stats.IsEmpty());
  EXPECT_EQ(stats.since, 1000);

  EXPECT_EQ(stats.listened_ms, 4000);
  EXPECT_EQ(stats.local_ms, 1500);
  EXPECT_EQ(stats.stream_ms, 2500);
  EXPECT_EQ(stats.songs["/song.mp3"].listened_ms, 1500);
  EXPECT_EQ(stats.songs["https://site/mix"].listened_ms, 2500);

  // Song is counted as played on its own
  EXPECT_EQ(stats.songs_played, 0);
  stats.AddPlayed("/song.mp3", 2000);
  EXPECT_EQ(stats.songs_played, 1);
  EXPECT_EQ(stats.songs["/song.mp3"].plays, 1);

  // Listening to the same song again keeps its totals (with its latest information)
  stats.AddListened("/song.mp3", "Artist", "Song (Remastered)", false, 500, 3000);
  EXPECT_EQ(stats.songs["/song.mp3"].listened_ms, 2000);
  EXPECT_EQ(stats.songs["/song.mp3"].title, "Song (Remastered)");
  EXPECT_EQ(stats.since, 1000);
}

/* ********************************************************************************************** */

TEST(ListeningStatsTest, KeepOnlyMostListenedSongs) {
  model::ListeningStats stats;

  // Song N was listened to for N + 1 milliseconds
  for (std::size_t i = 0; i < model::ListeningStats::kMaxSongs; i++) {
    const std::string key = "/song" + std::to_string(i);
    stats.AddListened(key, "", key, false, i + 1, 1);
  }

  ASSERT_THAT(stats.songs, SizeIs(model::ListeningStats::kMaxSongs));

  // Song already kept is not removed when listened to again
  stats.AddListened("/song0", "", "/song0", false, 1000, 1);
  EXPECT_THAT(stats.songs, SizeIs(model::ListeningStats::kMaxSongs));
  EXPECT_EQ(stats.songs.count("/song0"), 1);

  // But the least listened one gives room to a new one
  stats.AddListened("/new", "", "/new", false, 1, 1);
  EXPECT_THAT(stats.songs, SizeIs(model::ListeningStats::kMaxSongs));
  EXPECT_EQ(stats.songs.count("/song1"), 0);
  EXPECT_EQ(stats.songs.count("/song0"), 1);
  EXPECT_EQ(stats.songs.count("/new"), 1);
}

/* ********************************************************************************************** */

TEST(ListeningStatsTest, GetTopArtistsAndSongs) {
  model::ListeningStats stats;

  auto listen = [&stats](const std::string& key, const std::string& artist, uint64_t ms) {
    stats.AddListened(key, artist, key, false, ms, 1);
  };

  listen("a1", "First", 100);
  listen("a2", "First", 250);
  listen("b1", "Second", 300);
  listen("c1", "", 1000);
  listen("d1", "Third", 0);

  // Artists sum their songs, and songs without artist or not listened to are not included
  const auto artists = stats.GetTopArtists(5);
  ASSERT_THAT(artists, SizeIs(2));
  EXPECT_EQ(artists[0].name, "First");
  EXPECT_EQ(artists[0].listened_ms, 350);
  EXPECT_EQ(artists[1].name, "Second");

  const auto songs = stats.GetTopSongs(5);
  ASSERT_THAT(songs, SizeIs(4));
  EXPECT_EQ(songs[0].title, "c1");
  EXPECT_EQ(songs[1].title, "b1");
  EXPECT_EQ(songs[2].title, "a2");
  EXPECT_EQ(songs[3].title, "a1");

  // Only asked number of entries
  EXPECT_THAT(stats.GetTopSongs(2), SizeIs(2));
  EXPECT_THAT(stats.GetTopArtists(1), SizeIs(1));
}

/* ********************************************************************************************** */

TEST(ListeningStatsTest, AddWhatWasCountedSomewhereElse) {
  // Stats saved by this instance
  model::ListeningStats saved;
  saved.AddListened("/song.mp3", "Artist", "Song", false, 1000, 2000);
  saved.AddPlayed("/song.mp3", 2000);

  // Then this instance counts a bit more
  model::ListeningStats current = saved;
  current.AddListened("/song.mp3", "Artist", "Song", false, 500, 3000);
  current.AddListened("/other.mp3", "", "Other", false, 200, 3000);
  current.AddPlayed("/other.mp3", 3000);
  current.songs_skipped++;

  const model::ListeningStats diff = current.Difference(saved);
  EXPECT_EQ(diff.since, 0);
  EXPECT_EQ(diff.listened_ms, 700);
  EXPECT_EQ(diff.songs_played, 1);
  EXPECT_EQ(diff.songs_skipped, 1);
  ASSERT_THAT(diff.songs, SizeIs(2));
  EXPECT_EQ(diff.songs.at("/song.mp3").listened_ms, 500);
  EXPECT_EQ(diff.songs.at("/song.mp3").plays, 0);
  EXPECT_EQ(diff.songs.at("/other.mp3").plays, 1);

  // While another instance saved stats from an earlier moment, with another song
  model::ListeningStats latest;
  latest.AddListened("/song.mp3", "Artist", "Song", false, 4000, 1000);
  latest.AddListened("https://site/mix", "", "Mix", true, 3000, 1000);
  latest.AddPlayed("https://site/mix", 1000);

  // So only what was counted by this instance is added to them
  latest.Add(diff);
  EXPECT_EQ(latest.since, 1000);
  EXPECT_EQ(latest.listened_ms, 7700);
  EXPECT_EQ(latest.local_ms, 4700);
  EXPECT_EQ(latest.stream_ms, 3000);
  EXPECT_EQ(latest.songs_played, 2);
  EXPECT_EQ(latest.songs_skipped, 1);
  EXPECT_EQ(latest.songs.at("/song.mp3").listened_ms, 4500);
  EXPECT_EQ(latest.songs.at("/other.mp3").listened_ms, 200);
  EXPECT_EQ(latest.songs.at("https://site/mix").plays, 1);

  // Nothing counted means nothing to add
  EXPECT_EQ(saved.Difference(saved), model::ListeningStats{});

  // Without stats saved before, everything counted is added
  EXPECT_EQ(current.Difference(model::ListeningStats{}).since, 2000);
}

/* ********************************************************************************************** */

//! Set time zone while it exists (POSIX format, so it does not depend on time zone database)
class TimeZone {
 public:
  explicit TimeZone(const char* zone) {
    if (const char* current = std::getenv("TZ"); current) previous_ = current;
    setenv("TZ", zone, 1);
    tzset();
  }

  ~TimeZone() {
    if (previous_) {
      setenv("TZ", previous_->c_str(), 1);
    } else {
      unsetenv("TZ");
    }
    tzset();
  }

 private:
  std::optional<std::string> previous_;
};

/* ********************************************************************************************** */

TEST(ListeningStatsTest, CountListenedByWeekdayAndHour) {
  constexpr int64_t kMonday = 1793579400;  // Mon, Nov 2 2026 00:30 UTC
  constexpr int64_t kSunday = 1794182340;  // Sun, Nov 8 2026 23:59 UTC

  const TimeZone utc{"UTC0"};
  model::ListeningStats stats;

  // Week starts on Monday
  stats.AddListened("/song.mp3", "", "Song", false, 1000, kMonday);
  stats.AddListened("/song.mp3", "", "Song", false, 500, kMonday);
  stats.AddListened("/song.mp3", "", "Song", false, 300, kSunday);

  EXPECT_EQ(stats.hours[0][0], 1500);
  EXPECT_EQ(stats.hours[6][23], 300);

  // Weekday and hour come from local time
  const TimeZone local{"<-03>3"};
  stats.AddListened("/song.mp3", "", "Song", false, 200, kMonday);
  EXPECT_EQ(stats.hours[6][21], 200);
  EXPECT_EQ(stats.hours[0][0], 1500);

  // Only hours from each moment were counted
  uint64_t total = 0;
  for (const auto& day : stats.hours) {
    for (uint64_t listened : day) total += listened;
  }

  EXPECT_EQ(total, stats.listened_ms);
}

/* ********************************************************************************************** */

TEST(ListeningStatsTest, CountDaysInARowWithSongsPlayed) {
  constexpr int64_t kDay = 24 * 60 * 60;
  constexpr int64_t kMonday = 1793579400;  // Mon, Nov 2 2026 00:30 UTC

  const TimeZone utc{"UTC0"};
  model::ListeningStats stats;
  EXPECT_EQ(stats.GetCurrentStreak(kMonday), 0);

  // Each day counts once
  stats.AddPlayed("/song.mp3", kMonday);
  stats.AddPlayed("/song.mp3", kMonday + 60);
  EXPECT_EQ(stats.streak.current, 1);
  EXPECT_EQ(stats.streak.longest, 1);

  stats.AddPlayed("/song.mp3", kMonday + kDay);
  stats.AddPlayed("/song.mp3", kMonday + 2 * kDay);
  EXPECT_EQ(stats.streak.current, 3);
  EXPECT_EQ(stats.streak.longest, 3);

  // Streak goes on until a whole day goes by without songs played
  EXPECT_EQ(stats.GetCurrentStreak(kMonday + 2 * kDay), 3);
  EXPECT_EQ(stats.GetCurrentStreak(kMonday + 3 * kDay), 3);
  EXPECT_EQ(stats.GetCurrentStreak(kMonday + 4 * kDay), 0);

  // Then it starts again, keeping the longest one
  stats.AddPlayed("/song.mp3", kMonday + 4 * kDay);
  EXPECT_EQ(stats.streak.current, 1);
  EXPECT_EQ(stats.streak.longest, 3);

  // Clock going back does not change it
  stats.AddPlayed("/song.mp3", kMonday);
  EXPECT_EQ(stats.streak.current, 1);
  EXPECT_EQ(stats.streak.day, (kMonday + 4 * kDay) / kDay);

  // Day comes from local time (00:30 UTC is still the day before)
  model::ListeningStats local_stats;
  const TimeZone local{"<-03>3"};
  local_stats.AddPlayed("/song.mp3", kMonday);
  local_stats.AddPlayed("/song.mp3", kMonday + 4 * 60 * 60);
  EXPECT_EQ(local_stats.streak.current, 2);
}

/* ********************************************************************************************** */

TEST(ListeningStatsTest, AddHoursAndStreakCountedSomewhereElse) {
  constexpr int64_t kDay = 24 * 60 * 60;
  constexpr int64_t kMonday = 1793579400;  // Mon, Nov 2 2026 00:30 UTC

  const TimeZone utc{"UTC0"};

  // Stats saved by this instance (streak of two days)
  model::ListeningStats saved;
  saved.AddListened("/song.mp3", "", "Song", false, 1000, kMonday);
  saved.AddPlayed("/song.mp3", kMonday);
  saved.AddPlayed("/song.mp3", kMonday + kDay);

  // Then this instance listens a bit more on the next day
  model::ListeningStats current = saved;
  current.AddListened("/song.mp3", "", "Song", false, 500, kMonday + 2 * kDay);
  current.AddPlayed("/song.mp3", kMonday + 2 * kDay);

  // Hours are added like any other total, but streak is carried as it is
  const model::ListeningStats diff = current.Difference(saved);
  EXPECT_EQ(diff.hours[0][0], 0);
  EXPECT_EQ(diff.hours[2][0], 500);
  EXPECT_EQ(diff.streak, current.streak);

  // Another instance saved a longer streak, but it ended before
  model::ListeningStats latest = saved;
  latest.streak = {.day = saved.streak.day - 1, .current = 1, .longest = 10};
  latest.Add(diff);
  EXPECT_EQ(latest.hours[0][0], 1000);
  EXPECT_EQ(latest.hours[2][0], 500);
  EXPECT_EQ(latest.streak.day, current.streak.day);
  EXPECT_EQ(latest.streak.current, 3);
  EXPECT_EQ(latest.streak.longest, 10);

  // Both played songs on the same day, so the longest streak until then is kept
  latest.Add(
      model::ListeningStats{.streak = {.day = latest.streak.day, .current = 5, .longest = 5}});
  EXPECT_EQ(latest.streak.current, 5);

  // Streak from an older day is not the current one
  latest.Add(model::ListeningStats{.streak = {.day = 1, .current = 20, .longest = 20}});
  EXPECT_EQ(latest.streak.current, 5);
  EXPECT_EQ(latest.streak.longest, 20);

  // Streak not changed is not carried
  EXPECT_EQ(saved.Difference(saved).streak, model::ListeningStats::Streak{});
}

/* ********************************************************************************************** */

/**
 * @brief Tests with FileSink class (using a temporary directory for log files)
 */
class FileSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir = std::filesystem::temp_directory_path() / "spectrum_sink_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    path = (dir / "spectrum.log").string();
  }

  void TearDown() override { std::filesystem::remove_all(dir); }

  //! Write content to the given file
  static void WriteFile(const std::string& filepath, const std::string& content) {
    std::ofstream(filepath) << content;
  }

  //! Read content from the given file (empty if it does not exist)
  static std::string ReadFile(const std::string& filepath) {
    std::ifstream in(filepath);
    return std::string(std::istreambuf_iterator<char>(in), {});
  }

  //! Write a message to log file using a sink with the given maximum size
  void Log(const std::string& message, std::uintmax_t max_size) const {
    util::FileSink sink(path, max_size);
    sink.OpenStream();
    sink << message;
  }

  static constexpr std::uintmax_t kMaxSize = 100;  //!< Maximum size for log file in tests

  std::filesystem::path dir;  //!< Temporary directory
  std::string path;           //!< Log file path
};

/* ********************************************************************************************** */

TEST_F(FileSinkTest, AppendWhileBelowMaximumSize) {
  WriteFile(path, "old\n");

  Log("new\n", kMaxSize);

  EXPECT_THAT(ReadFile(path), StrEq("old\nnew\n"));
  EXPECT_FALSE(std::filesystem::exists(path + ".1"));
}

/* ********************************************************************************************** */

TEST_F(FileSinkTest, RotateWhenMaximumSizeIsReached) {
  const std::string old_content(kMaxSize, 'x');
  WriteFile(path, old_content);
  WriteFile(path + ".1", "oldest\n");

  Log("new\n", kMaxSize);

  // Previous log file replaces the oldest one, and a new log file is started
  EXPECT_THAT(ReadFile(path + ".1"), StrEq(old_content));
  EXPECT_THAT(ReadFile(path), StrEq("new\n"));
}

/* ********************************************************************************************** */

TEST(ProcessTest, FindExecutable) {
  auto shell = util::FindExecutable("sh");
  ASSERT_TRUE(shell.has_value());
  EXPECT_EQ(shell->filename(), "sh");

  EXPECT_FALSE(util::FindExecutable("spectrum-program-that-does-not-exist").has_value());
}

/* ********************************************************************************************** */

TEST(ProcessTest, RunProcessAndCaptureOutput) {
  constexpr std::chrono::seconds kTimeout{5};

  auto result = util::RunProcess({"sh", "-c", "echo out; echo err >&2; exit 3"}, kTimeout);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->exit_code, 3);
  EXPECT_FALSE(result->timed_out);
  EXPECT_THAT(result->output, StrEq("out\n"));
  EXPECT_THAT(result->error, StrEq("err\n"));

  // Program that cannot be started
  EXPECT_FALSE(util::RunProcess({"spectrum-program-that-does-not-exist"}, kTimeout).has_value());
}

/* ********************************************************************************************** */

TEST(ProcessTest, StartProcessWithoutWaiting) {
  constexpr std::chrono::seconds kTimeout{5};
  constexpr std::chrono::milliseconds kCheckInterval{20};

  const auto file = std::filesystem::temp_directory_path() / "spectrum_start_process_test";
  std::filesystem::remove(file);

  // Returns right away, while program is still running (it writes to file only after a while)
  const auto start = std::chrono::steady_clock::now();
  ASSERT_TRUE(util::StartProcess({"sh", "-c", "sleep 0.2; echo started > \"$0\"", file.string()}));
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(150));
  EXPECT_FALSE(std::filesystem::exists(file));

  while (!std::filesystem::exists(file) && std::chrono::steady_clock::now() - start < kTimeout) {
    std::this_thread::sleep_for(kCheckInterval);
  }

  EXPECT_TRUE(std::filesystem::exists(file));
  std::filesystem::remove(file);

  // Program that cannot be started
  EXPECT_FALSE(util::StartProcess({"spectrum-program-that-does-not-exist"}));
  EXPECT_FALSE(util::StartProcess({}));
}

/* ********************************************************************************************** */

TEST(ProcessTest, KillProcessAfterTimeout) {
  auto result = util::RunProcess({"sh", "-c", "sleep 5"}, std::chrono::milliseconds{100});

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->timed_out);
  EXPECT_NE(result->exit_code, 0);
}

/* ********************************************************************************************** */

TEST(ProcessTest, KillProcessWhenCanceled) {
  std::atomic<bool> cancel = false;

  // Cancel while program is still running
  std::thread canceler([&cancel] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cancel = true;
  });

  const auto start = std::chrono::steady_clock::now();
  auto result = util::RunProcess({"sh", "-c", "sleep 5"}, std::chrono::seconds(5), &cancel);
  const auto elapsed = std::chrono::steady_clock::now() - start;

  canceler.join();

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->canceled);
  EXPECT_FALSE(result->timed_out);
  EXPECT_LT(elapsed, std::chrono::seconds(2));
}

/* ********************************************************************************************** */

TEST(RemoteCommandTest, ParseAndPrintNames) {
  for (const auto& [command, name] : model::kRemoteCommands) {
    EXPECT_EQ(model::ParseRemoteCommand(name), command) << name;
    EXPECT_EQ(model::GetRemoteCommandName(command), name);
  }

  EXPECT_FALSE(model::ParseRemoteCommand("").has_value());
  EXPECT_FALSE(model::ParseRemoteCommand("Next").has_value());
  EXPECT_FALSE(model::ParseRemoteCommand("explode").has_value());

  EXPECT_THAT(model::GetRemoteCommandNames(), ::testing::StartsWith("play-pause, play, pause, "));

  // Status is not a command, but it is also available from command-line
  EXPECT_FALSE(model::ParseRemoteCommand(model::kRemoteStatusQuery).has_value());
  EXPECT_FALSE(model::ParseRemoteCommand(model::kRemoteSubscribeQuery).has_value());
  EXPECT_THAT(model::GetRemoteCommandNames(), ::testing::EndsWith(", quit, status, subscribe"));
}

/* ********************************************************************************************** */

TEST(RemoteCommandTest, ParseRequestWithValue) {
  using model::RemoteCommand;
  using model::RemoteNumber;
  using model::RemoteRequest;
  using model::RepeatMode;

  std::string error;

  const auto parse = [&error](const std::string& text) {
    error.clear();
    return model::ParseRemoteRequest(text, error);
  };

  //! Text that must be accepted, with the request expected from it
  const std::vector<std::pair<std::string, RemoteRequest>> valid{
      {"next", RemoteCommand::SkipToNext},
      {"  quit  ", RemoteCommand::Quit},
      {"volume 50", {RemoteCommand::SetVolume, RemoteNumber{50, false}}},
      {"volume 0", {RemoteCommand::SetVolume, RemoteNumber{0, false}}},
      {"volume   100", {RemoteCommand::SetVolume, RemoteNumber{100, false}}},
      {"volume +5", {RemoteCommand::SetVolume, RemoteNumber{5, true}}},
      {"volume -150", {RemoteCommand::SetVolume, RemoteNumber{-150, true}}},
      {"seek 90", {RemoteCommand::Seek, RemoteNumber{90, false}}},
      {"seek 1:30", {RemoteCommand::Seek, RemoteNumber{90, false}}},
      {"seek 90:00", {RemoteCommand::Seek, RemoteNumber{5400, false}}},
      {"seek 1:02:03", {RemoteCommand::Seek, RemoteNumber{3723, false}}},
      {"seek +10", {RemoteCommand::Seek, RemoteNumber{10, true}}},
      {"seek -1:05", {RemoteCommand::Seek, RemoteNumber{-65, true}}},
      {"repeat", RemoteCommand::ToggleRepeat},
      {"repeat off", {RemoteCommand::ToggleRepeat, RepeatMode::Off}},
      {"repeat all", {RemoteCommand::ToggleRepeat, RepeatMode::All}},
      {"repeat one", {RemoteCommand::ToggleRepeat, RepeatMode::One}},
      {"shuffle", RemoteCommand::ToggleShuffle},
      {"shuffle on", {RemoteCommand::ToggleShuffle, true}},
      {"shuffle off", {RemoteCommand::ToggleShuffle, false}},
      {"play", RemoteCommand::Play},
      {"play /path/with  some spaces.mp3 ",
       {RemoteCommand::Play, std::string{"/path/with  some spaces.mp3"}}},
      {"play https://www.youtube.com/watch?v=abc",
       {RemoteCommand::Play, std::string{"https://www.youtube.com/watch?v=abc"}}},
  };

  for (const auto& [text, expected] : valid) {
    auto request = parse(text);
    ASSERT_TRUE(request.has_value()) << text << ": " << error;
    EXPECT_EQ(*request, expected) << text;
    EXPECT_THAT(error, IsEmpty());

    // Request is sent as text to the running instance, which must get the same from it
    std::ostringstream sent;
    sent << *request;

    auto received = parse(sent.str());
    ASSERT_TRUE(received.has_value()) << sent.str() << ": " << error;
    EXPECT_EQ(*received, expected) << sent.str();
  }

  //! Text that must be refused, with part of the reason for it
  const std::vector<std::pair<std::string, std::string>> invalid{
      {"", "unknown command \"\""},
      {"explode now", "unknown command \"explode\""},
      {"next 2", "command \"next\" does not accept a value"},
      {"quit now", "command \"quit\" does not accept a value"},
      {"volume", "missing value for command \"volume\""},
      {"volume 101", "invalid value \"101\" for command \"volume\""},
      {"volume 5%", "invalid value \"5%\""},
      {"volume + 5", "invalid value \"+ 5\""},
      {"volume 1234567", "invalid value \"1234567\""},
      {"seek", "missing value for command \"seek\""},
      {"seek 1:60", "invalid value \"1:60\" for command \"seek\""},
      {"seek 1:2:3:4", "invalid value \"1:2:3:4\""},
      {"seek 1:", "invalid value \"1:\""},
      {"seek abc", "invalid value \"abc\""},
      {"repeat two", "invalid value \"two\" for command \"repeat\" (expected off, all or one)"},
      {"shuffle yes", "invalid value \"yes\" for command \"shuffle\" (expected on or off)"},
  };

  for (const auto& [text, reason] : invalid) {
    EXPECT_FALSE(parse(text).has_value()) << text;
    EXPECT_THAT(error, ::testing::HasSubstr(reason)) << text;
  }
}

/* ********************************************************************************************** */

TEST(PlayerStatusTest, ConvertToJson) {
  // Nothing is playing
  EXPECT_THAT(model::to_json(model::PlayerStatus{}),
              StrEq(R"({"artist":"","duration":0,"muted":false,"output_bit_depth":0,)"
                    R"("output_device":"","output_sample_rate":0,"position":0,"repeat":"off",)"
                    R"("shuffle":false,"state":"stopped","title":"","volume":100})"));

  model::PlayerStatus status{
      .state = model::Song::MediaState::Play,
      .artist = "Deko \"Tok\"",
      .title = "First line\nSecond line",
      .position = 75,
      .duration = 3725,
      .output =
          model::AudioOutput{
              .device = "front:CARD=DAC,DEV=0",
              .format = model::AudioFormat{.sample_rate = 96000,
                                           .sample_format = model::SampleFormat::S32},
          },
      .volume = model::Volume{0.35F},
      .repeat = model::RepeatMode::All,
      .shuffle = true,
  };
  status.volume.ToggleMute();

  // Always a single line, no matter the content
  EXPECT_THAT(
      model::to_json(status),
      StrEq(R"({"artist":"Deko \"Tok\"","duration":3725,"muted":true,"output_bit_depth":32,)"
            R"("output_device":"front:CARD=DAC,DEV=0","output_sample_rate":96000,"position":75,)"
            R"("repeat":"all","shuffle":true,"state":"playing","title":"First line\nSecond line",)"
            R"("volume":35})"));

  status.state = model::Song::MediaState::Pause;
  EXPECT_THAT(model::to_json(status), ::testing::HasSubstr(R"("state":"paused")"));

  // Invalid text from metadata is replaced
  status.title = "Invalid \xff text";
  EXPECT_THAT(model::to_json(status),
              ::testing::HasSubstr("\"title\":\"Invalid \xEF\xBF\xBD text\""));
}

/* ********************************************************************************************** */

TEST(PlayerStatusTest, FormatAsText) {
  model::PlayerStatus status{
      .state = model::Song::MediaState::Play,
      .artist = "Deko",
      .title = "Use {volume} wisely",
      .position = 75,
      .duration = 3725,
      .volume = model::Volume{0.35F},
      .repeat = model::RepeatMode::One,
      .shuffle = true,
  };

  const std::string json = model::to_json(status);

  // Without a format, it is kept as JSON
  auto text = model::format_status(json, std::nullopt);
  ASSERT_TRUE(text.has_value());
  EXPECT_THAT(*text, StrEq(json));

  // Values are not formatted again, even when they look like a field
  text = model::format_status(json, "{artist} - {title} [{position}/{duration}]");
  ASSERT_TRUE(text.has_value());
  EXPECT_THAT(*text, StrEq("Deko - Use {volume} wisely [01:15/01:02:05]"));

  text = model::format_status(json, "{state} vol:{volume}% muted:{muted} {repeat} {shuffle}");
  ASSERT_TRUE(text.has_value());
  EXPECT_THAT(*text, StrEq("playing vol:35% muted:off one on"));

  // Anything that is not a field is kept
  text = model::format_status(json, "{unknown} {{state}} {state {artist");
  ASSERT_TRUE(text.has_value());
  EXPECT_THAT(*text, StrEq("{unknown} {playing} {state {artist"));

  text = model::format_status(json, "");
  ASSERT_TRUE(text.has_value());
  EXPECT_THAT(*text, StrEq(""));

  // Reply from running instance is an error message
  EXPECT_FALSE(model::format_status("unknown command \"status\"", "{title}").has_value());
  EXPECT_FALSE(model::format_status("ok", std::nullopt).has_value());
  EXPECT_FALSE(model::format_status("\"text\"", "{title}").has_value());
}

/* ********************************************************************************************** */

TEST(MprisTest, ConvertStatus) {
  using model::Song;

  EXPECT_THAT(model::GetMprisPlaybackStatus(Song::MediaState::Play), StrEq("Playing"));
  EXPECT_THAT(model::GetMprisPlaybackStatus(Song::MediaState::Pause), StrEq("Paused"));
  EXPECT_THAT(model::GetMprisPlaybackStatus(Song::MediaState::Stop), StrEq("Stopped"));
  EXPECT_THAT(model::GetMprisPlaybackStatus(Song::MediaState::Empty), StrEq("Stopped"));
  EXPECT_THAT(model::GetMprisPlaybackStatus(Song::MediaState::Finished), StrEq("Stopped"));

  for (auto mode : {model::RepeatMode::Off, model::RepeatMode::All, model::RepeatMode::One}) {
    EXPECT_EQ(model::ParseMprisLoopStatus(model::GetMprisLoopStatus(mode)), mode);
  }

  EXPECT_THAT(model::GetMprisLoopStatus(model::RepeatMode::One), StrEq("Track"));
  EXPECT_FALSE(model::ParseMprisLoopStatus("track").has_value());

  // Each song loaded has its own identifier
  model::PlayerStatus status;
  EXPECT_THAT(model::GetMprisTrackId(status), StrEq(model::kMprisNoTrack));

  status.title = "Song";
  status.song_count = 7;
  EXPECT_THAT(model::GetMprisTrackId(status), StrEq("/org/spectrum/track/7"));

  EXPECT_EQ(model::ToMprisTime(90), 90'000'000);
}

/* ********************************************************************************************** */

TEST(MprisTest, ConvertLocation) {
  using ::testing::IsEmpty;

  // Local file has special characters encoded (including any byte from UTF-8)
  EXPECT_THAT(model::ToMprisFileUrl("/home/me/My Songs/a+b #1 (ação).mp3"),
              StrEq("file:///home/me/My%20Songs/a%2Bb%20%231%20%28a%C3%A7%C3%A3o%29.mp3"));

  // And it is decoded back to the same path
  const std::string path = "/home/me/100% [live]/~song_1-2.flac";
  EXPECT_EQ(model::CreateMprisOpenUri(model::ToMprisFileUrl(path)),
            model::RemoteRequest(model::RemoteCommand::Play, path));

  model::PlayerStatus status;
  EXPECT_THAT(model::GetMprisUrl(status), IsEmpty());

  status.location = "/music/song.mp3";
  EXPECT_THAT(model::GetMprisUrl(status), StrEq("file:///music/song.mp3"));

  // Stream keeps its original URL
  status.location = "https://www.youtube.com/watch?v=abc&t=10";
  EXPECT_THAT(model::GetMprisUrl(status), StrEq("https://www.youtube.com/watch?v=abc&t=10"));

  EXPECT_THAT(model::GetMprisImageExtension(std::string{"\x89PNG\r\n\x1a\n", 8}), StrEq(".png"));
  EXPECT_THAT(model::GetMprisImageExtension("\xff\xd8\xff\xe0"), StrEq(".jpg"));
  EXPECT_THAT(model::GetMprisImageExtension(""), StrEq(".jpg"));
}

/* ********************************************************************************************** */

TEST(MprisTest, CreateRequests) {
  using model::RemoteCommand;
  using model::RemoteNumber;
  using model::RemoteRequest;

  // Offset is rounded to seconds
  EXPECT_EQ(model::CreateMprisSeek(5'400'000),
            RemoteRequest(RemoteCommand::Seek, RemoteNumber{.value = 5, .relative = true}));
  EXPECT_EQ(model::CreateMprisSeek(-10'000'000),
            RemoteRequest(RemoteCommand::Seek, RemoteNumber{.value = -10, .relative = true}));
  EXPECT_FALSE(model::CreateMprisSeek(400'000).has_value());

  model::PlayerStatus status{
      .state = model::Song::MediaState::Play, .title = "Song", .duration = 200, .song_count = 3};
  const std::string track = model::GetMprisTrackId(status);

  EXPECT_EQ(model::CreateMprisSetPosition(status, track, 30'000'000),
            RemoteRequest(RemoteCommand::Seek, RemoteNumber{.value = 30, .relative = false}));

  // Ignored for another song, or a position outside of it
  EXPECT_FALSE(model::CreateMprisSetPosition(status, "/org/spectrum/track/2", 0).has_value());
  EXPECT_FALSE(model::CreateMprisSetPosition(status, track, -1).has_value());
  EXPECT_FALSE(model::CreateMprisSetPosition(status, track, 201'000'000).has_value());

  EXPECT_EQ(model::CreateMprisSetVolume(0.355),
            RemoteRequest(RemoteCommand::SetVolume, RemoteNumber{.value = 36, .relative = false}));
  EXPECT_EQ(model::CreateMprisSetVolume(1.5),
            RemoteRequest(RemoteCommand::SetVolume, RemoteNumber{.value = 100, .relative = false}));
  EXPECT_EQ(model::CreateMprisSetVolume(-1),
            RemoteRequest(RemoteCommand::SetVolume, RemoteNumber{.value = 0, .relative = false}));

  // Local file is played by its path
  EXPECT_EQ(model::CreateMprisOpenUri("file:///home/me/My%20Songs/a%2Bb.flac"),
            RemoteRequest(RemoteCommand::Play, std::string{"/home/me/My Songs/a+b.flac"}));
  EXPECT_EQ(
      model::CreateMprisOpenUri("https://www.youtube.com/watch?v=abc%20"),
      RemoteRequest(RemoteCommand::Play, std::string{"https://www.youtube.com/watch?v=abc%20"}));

  EXPECT_FALSE(model::CreateMprisOpenUri("file://host/song.mp3").has_value());
  EXPECT_FALSE(model::CreateMprisOpenUri("file:///song%2.mp3").has_value());
  EXPECT_FALSE(model::CreateMprisOpenUri("file:///song%").has_value());
  EXPECT_FALSE(model::CreateMprisOpenUri("ftp://host/song.mp3").has_value());
  EXPECT_FALSE(model::CreateMprisOpenUri("/home/me/song.mp3").has_value());
}

/* ********************************************************************************************** */

TEST(MprisTest, NotifyChanges) {
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;

  model::PlayerStatus before{.state = model::Song::MediaState::Play,
                             .artist = "Artist",
                             .title = "Song",
                             .position = 10,
                             .duration = 200,
                             .song_count = 1};

  // Position moving forward while playing is not notified
  auto after = before;
  after.position = 11;
  auto changes = model::GetMprisChanges(before, after);
  EXPECT_THAT(changes.properties, IsEmpty());
  EXPECT_FALSE(changes.seeked);

  after.position = 60;
  changes = model::GetMprisChanges(before, after);
  EXPECT_THAT(changes.properties, IsEmpty());
  EXPECT_TRUE(changes.seeked);

  after.position = 5;
  EXPECT_TRUE(model::GetMprisChanges(before, after).seeked);

  // Position does not move while paused
  before.state = model::Song::MediaState::Pause;
  after = before;
  after.position = 11;
  EXPECT_TRUE(model::GetMprisChanges(before, after).seeked);

  after.state = model::Song::MediaState::Play;
  after.position = 10;
  changes = model::GetMprisChanges(before, after);
  EXPECT_THAT(changes.properties, ElementsAre(StrEq(model::kMprisPlaybackStatus)));
  EXPECT_FALSE(changes.seeked);

  // A new song starts from the beginning, even if it is the same song again
  after = before;
  after.state = model::Song::MediaState::Play;
  after.position = 0;
  after.song_count = 2;
  changes = model::GetMprisChanges(before, after);
  EXPECT_THAT(changes.properties,
              ElementsAre(StrEq(model::kMprisPlaybackStatus), StrEq(model::kMprisMetadata)));
  EXPECT_FALSE(changes.seeked);

  // Cover art changes metadata
  after = before;
  after.cover = std::make_shared<const std::string>("image");
  EXPECT_THAT(model::GetMprisChanges(before, after).properties,
              ElementsAre(StrEq(model::kMprisMetadata)));

  // Song stopped also starts from the beginning
  after = before;
  after.state = model::Song::MediaState::Stop;
  after.position = 0;
  EXPECT_FALSE(model::GetMprisChanges(before, after).seeked);

  // Without a song (or a live stream), position cannot be changed
  after = model::PlayerStatus{
      .volume = model::Volume{0.5F}, .repeat = model::RepeatMode::One, .shuffle = true};
  changes = model::GetMprisChanges(before, after);
  EXPECT_THAT(changes.properties,
              ElementsAre(StrEq(model::kMprisPlaybackStatus), StrEq(model::kMprisLoopStatus),
                          StrEq(model::kMprisShuffle), StrEq(model::kMprisVolume),
                          StrEq(model::kMprisMetadata), StrEq(model::kMprisCanSeek)));
}

/* ********************************************************************************************** */

/**
 * @brief Tests with RemoteServer class (using a socket in a temporary directory)
 */
class RemoteTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory = std::filesystem::temp_directory_path() / "spectrum_remote_test";
    std::filesystem::remove_all(directory);

    path = (directory / "spectrum.sock").string();
  }

  void TearDown() override { std::filesystem::remove_all(directory); }

  //! Create directory for socket with the given permissions
  void CreateDirectory(std::filesystem::perms permissions) {
    std::filesystem::create_directories(directory);
    std::filesystem::permissions(directory, permissions);
  }

  std::filesystem::path directory;  //!< Directory for socket
  std::string path;                 //!< Socket path
};

/* ********************************************************************************************** */

TEST_F(RemoteTest, SendRequestAndReceiveReply) {
  std::vector<std::string> received;

  auto server = util::RemoteServer::Create(path, [&received](const std::string& request) {
    received.push_back(request);
    return "reply to " + request;
  });
  ASSERT_NE(server, nullptr);

  // Directory is created by server, and both of them are accessible only by the current user
  using std::filesystem::perms;
  EXPECT_EQ(std::filesystem::status(directory).permissions(), perms::owner_all);
  EXPECT_EQ(std::filesystem::status(path).permissions(), perms::owner_read | perms::owner_write);

  auto reply = util::SendRemoteRequest(path, "next");
  ASSERT_TRUE(reply.has_value());
  EXPECT_THAT(*reply, StrEq("reply to next"));

  reply = util::SendRemoteRequest(path, "stop");
  ASSERT_TRUE(reply.has_value());
  EXPECT_THAT(*reply, StrEq("reply to stop"));

  // Socket is removed when server stops, and nobody replies anymore
  server->Stop();
  EXPECT_THAT(received, ::testing::ElementsAre("next", "stop"));
  EXPECT_FALSE(std::filesystem::exists(path));
  EXPECT_FALSE(util::SendRemoteRequest(path, "next").has_value());
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, ReplyBiggerThanRequest) {
  std::string content(util::kMaxRemoteRequestSize + 1, 'a');

  auto server =
      util::RemoteServer::Create(path, [&content](const std::string&) { return content; });
  ASSERT_NE(server, nullptr);

  // Player status does not fit in the size accepted for a request
  auto reply = util::SendRemoteRequest(path, "status");
  ASSERT_TRUE(reply.has_value());
  EXPECT_THAT(*reply, StrEq(content));

  // Request is discarded by server when it is too big
  EXPECT_FALSE(util::SendRemoteRequest(path, content).has_value());

  // And so is the reply by client
  content.assign(util::kMaxRemoteReplySize * 2, 'a');
  EXPECT_FALSE(util::SendRemoteRequest(path, "status").has_value());
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, PublishToSubscriber) {
  std::vector<std::string> requests;

  auto server = util::RemoteServer::Create(
      path,
      [&requests](const std::string& request) {
        requests.push_back(request);
        return "ok";
      },
      "subscribe");
  ASSERT_NE(server, nullptr);

  server->Publish("first");

  // Last content published is received right away, and then every new one (each one published
  // here only after receiving the previous, otherwise just the last of them would be sent)
  std::vector<std::string> updates;

  bool sent = util::ReceiveRemoteUpdates(path, "subscribe", [&](const std::string& update) {
    updates.push_back(update);

    if (update == "first") {
      server->Publish("first");
      server->Publish("second");
    } else if (update == "second") {
      server->Publish("third");
    }

    // Stop receiving
    return update != "third";
  });

  EXPECT_TRUE(sent);
  EXPECT_THAT(updates, ::testing::ElementsAre("first", "second", "third"));

  // Server keeps working after subscriber is gone, including for other requests
  server->Publish("fourth");

  auto reply = util::SendRemoteRequest(path, "next");
  ASSERT_TRUE(reply.has_value());
  EXPECT_THAT(*reply, StrEq("ok"));

  // Subscriber stops waiting when server stops
  updates.clear();

  sent = util::ReceiveRemoteUpdates(path, "subscribe", [&](const std::string& update) {
    updates.push_back(update);
    server->Stop();
    return true;
  });

  EXPECT_TRUE(sent);
  EXPECT_THAT(updates, ::testing::ElementsAre("fourth"));

  // Request to subscribe is never sent to handler
  EXPECT_THAT(requests, ::testing::ElementsAre("next"));

  // Nobody is listening anymore
  EXPECT_FALSE(
      util::ReceiveRemoteUpdates(path, "subscribe", [](const std::string&) { return true; }));
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, SubscribeWithoutSupportFromServer) {
  auto server = util::RemoteServer::Create(
      path, [](const std::string& request) { return "unknown command " + request; });
  ASSERT_NE(server, nullptr);

  // It is handled as any other request: a single reply is sent and connection is closed
  std::vector<std::string> updates;

  bool sent = util::ReceiveRemoteUpdates(path, "subscribe", [&](const std::string& update) {
    updates.push_back(update);
    return true;
  });

  EXPECT_TRUE(sent);
  EXPECT_THAT(updates, ::testing::ElementsAre("unknown command subscribe"));
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, OnlyFirstInstanceListens) {
  auto first = util::RemoteServer::Create(path, [](const std::string&) { return "first"; });
  ASSERT_NE(first, nullptr);

  auto second = util::RemoteServer::Create(path, [](const std::string&) { return "second"; });
  EXPECT_EQ(second, nullptr);

  // First instance keeps its socket
  auto reply = util::SendRemoteRequest(path, "next");
  ASSERT_TRUE(reply.has_value());
  EXPECT_THAT(*reply, StrEq("first"));
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, ReplaceSocketLeftByAnotherInstance) {
  // Socket file without anyone listening on it, as left by an instance that did not exit properly
  CreateDirectory(std::filesystem::perms::owner_all);
  std::ofstream(path) << "";
  ASSERT_TRUE(std::filesystem::exists(path));
  EXPECT_FALSE(util::SendRemoteRequest(path, "next").has_value());

  auto server = util::RemoteServer::Create(path, [](const std::string&) { return "ok"; });
  ASSERT_NE(server, nullptr);

  auto reply = util::SendRemoteRequest(path, "next");
  ASSERT_TRUE(reply.has_value());
  EXPECT_THAT(*reply, StrEq("ok"));
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, DirectoryAccessibleByOthers) {
  using std::filesystem::perms;
  const auto handler = [](const std::string&) { return "ok"; };

  // Someone else could replace the socket in this directory, so it is not used
  CreateDirectory(perms::owner_all | perms::group_all | perms::others_all);
  EXPECT_EQ(util::RemoteServer::Create(path, handler), nullptr);
  EXPECT_FALSE(std::filesystem::exists(path));

  // Even when there is a socket listening on it, request is not sent
  std::filesystem::permissions(directory, perms::owner_all);
  auto server = util::RemoteServer::Create(path, handler);
  ASSERT_NE(server, nullptr);
  ASSERT_TRUE(util::SendRemoteRequest(path, "next").has_value());

  std::filesystem::permissions(directory, perms::owner_all | perms::others_exec);
  EXPECT_FALSE(util::SendRemoteRequest(path, "next").has_value());

  // Same for a link to a directory
  std::filesystem::permissions(directory, perms::owner_all);
  const auto link = std::filesystem::temp_directory_path() / "spectrum_remote_test_link";
  std::filesystem::remove(link);
  std::filesystem::create_directory_symlink(directory, link);

  EXPECT_FALSE(util::SendRemoteRequest((link / "spectrum.sock").string(), "next").has_value());
  EXPECT_EQ(util::RemoteServer::Create((link / "other.sock").string(), handler), nullptr);

  std::filesystem::remove(link);
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, InvalidSocketPath) {
  const std::string too_long(512, 'a');

  EXPECT_EQ(util::RemoteServer::Create("", [](const std::string&) { return ""; }), nullptr);
  EXPECT_EQ(util::RemoteServer::Create(too_long, [](const std::string&) { return ""; }), nullptr);
  EXPECT_FALSE(util::SendRemoteRequest(too_long, "next").has_value());
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, SendCommandToRunningInstance) {
  std::vector<std::string> requests;

  auto server = util::RemoteServer::Create(path, [&requests](const std::string& request) {
    if (request == model::kRemoteStatusQuery) return model::to_json({.title = "Song"});

    requests.push_back(request);
    return request == "stop" ? std::string{"busy"} : std::string{model::kRemoteReplyOk};
  });
  ASSERT_NE(server, nullptr);

  std::ostringstream out;
  std::ostringstream err;

  const auto send = [&](const std::string& command,
                        const std::optional<std::string>& format = std::nullopt) {
    out.str("");
    err.str("");
    return middleware::SendRemoteCommand(path, command, format, out, err);
  };

  // Command is sent in a single format, no matter how it was written
  EXPECT_EQ(send("volume   50"), EXIT_SUCCESS);
  EXPECT_THAT(out.str(), IsEmpty());
  EXPECT_THAT(err.str(), IsEmpty());

  // Path is sent in full, as running instance has another working directory (anything else is
  // sent as it is, like the name of a playlist)
  EXPECT_EQ(send("play ."), EXIT_SUCCESS);
  EXPECT_EQ(send("play some playlist"), EXIT_SUCCESS);

  ASSERT_THAT(requests, SizeIs(3));
  EXPECT_THAT(requests[0], StrEq("volume 50"));
  EXPECT_THAT(requests[1], ::testing::HasSubstr(std::filesystem::current_path().string()));
  EXPECT_THAT(requests[2], ::testing::HasSubstr("some playlist"));

  // Status is printed using the given format
  EXPECT_EQ(send(std::string{model::kRemoteStatusQuery}, "title: {title}"), EXIT_SUCCESS);
  EXPECT_THAT(out.str(), StrEq("title: Song\n"));

  // Error replied by running instance is printed
  EXPECT_EQ(send("stop"), EXIT_FAILURE);
  EXPECT_THAT(err.str(), StrEq("spectrum: busy\n"));

  // Invalid commands are not sent, and all commands are listed only when it does not exist
  requests.clear();

  EXPECT_EQ(send("dance"), EXIT_FAILURE);
  EXPECT_THAT(err.str(), ::testing::HasSubstr("(available: "));

  EXPECT_EQ(send("volume loud"), EXIT_FAILURE);
  EXPECT_THAT(err.str(), Not(::testing::HasSubstr("(available: ")));

  EXPECT_THAT(requests, IsEmpty());

  // Nobody is listening anymore
  server->Stop();

  EXPECT_EQ(send("next"), EXIT_FAILURE);
  EXPECT_THAT(err.str(), StrEq("spectrum: there is no running instance to control\n"));
}

/* ********************************************************************************************** */

TEST_F(RemoteTest, PrintStatusUpdatesFromRunningInstance) {
  auto server = util::RemoteServer::Create(
      path, [](const std::string&) { return std::string{model::kRemoteReplyOk}; },
      std::string{model::kRemoteSubscribeQuery});
  ASSERT_NE(server, nullptr);

  server->Publish(model::to_json({.title = "Song"}));

  // Status is printed until running instance exits
  std::thread exit([&server]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    server->Stop();
  });

  std::ostringstream out;
  std::ostringstream err;

  int result = middleware::SendRemoteCommand(path, std::string{model::kRemoteSubscribeQuery},
                                             "{title}", out, err);
  exit.join();

  EXPECT_EQ(result, EXIT_SUCCESS);
  EXPECT_THAT(out.str(), StrEq("Song\n"));
  EXPECT_THAT(err.str(), IsEmpty());

  // Nobody is listening anymore
  result = middleware::SendRemoteCommand(path, std::string{model::kRemoteSubscribeQuery},
                                         std::nullopt, out, err);

  EXPECT_EQ(result, EXIT_FAILURE);
  EXPECT_THAT(err.str(), StrEq("spectrum: there is no running instance to control\n"));
}

/* ********************************************************************************************** */

TEST(LoopWakerTest, WakeUpOnlyWhenNotified) {
  using std::chrono::milliseconds;
  using std::chrono::steady_clock;

  util::LoopWaker waker;

  const auto wait = [&waker]() {
    auto start = steady_clock::now();
    waker.Wait();
    return std::chrono::duration_cast<milliseconds>(steady_clock::now() - start);
  };

  // Notified before waiting, so it does not wait at all
  waker.Notify();
  EXPECT_LT(wait(), milliseconds(util::LoopWaker::kMaxWaitMs / 2));

  // Notification was already handled, so it waits for the maximum time
  EXPECT_GE(wait(), milliseconds(util::LoopWaker::kMaxWaitMs - 5));

  // Notified by another thread while waiting
  std::thread notifier([&waker]() {
    std::this_thread::sleep_for(milliseconds(10));
    waker.Notify();
  });

  EXPECT_LT(wait(), milliseconds(util::LoopWaker::kMaxWaitMs - 5));
  notifier.join();
}

/* ********************************************************************************************** */

TEST(RemotePathTest, SocketPath) {
  std::optional<std::string> original;
  if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime) original = runtime;

  setenv("XDG_RUNTIME_DIR", "/run/user/1234", 1);
  EXPECT_THAT(util::GetRemoteSocketPath(), StrEq("/run/user/1234/spectrum.sock"));

  // Relative path is ignored
  setenv("XDG_RUNTIME_DIR", "relative/dir", 1);
  EXPECT_THAT(util::GetRemoteSocketPath(), ::testing::StartsWith("/tmp/spectrum-"));

  unsetenv("XDG_RUNTIME_DIR");
  EXPECT_THAT(util::GetRemoteSocketPath(), ::testing::AllOf(::testing::StartsWith("/tmp/spectrum-"),
                                                            ::testing::EndsWith("/spectrum.sock")));

  if (original) setenv("XDG_RUNTIME_DIR", original->c_str(), 1);
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ConfigDirectory) {
  // Default directory is inside home
  EXPECT_EQ(handler.GetConfigDirectory(), (home_dir / ".config" / "spectrum").string());
  EXPECT_EQ(handler.GetPlaylistsPath(), (home_dir / ".config/spectrum/playlists.json").string());
  EXPECT_EQ(handler.GetSettingsPath(), (home_dir / ".config/spectrum/settings.json").string());

  // Directory set by user is used instead
  const std::filesystem::path custom = home_dir / "custom";
  setenv("XDG_CONFIG_HOME", custom.c_str(), 1);
  EXPECT_EQ(handler.GetConfigDirectory(), (custom / "spectrum").string());
  EXPECT_EQ(handler.GetSettingsPath(), (custom / "spectrum/settings.json").string());

  // Unless it is not an absolute path
  setenv("XDG_CONFIG_HOME", "relative/path", 1);
  EXPECT_EQ(handler.GetConfigDirectory(), (home_dir / ".config" / "spectrum").string());

  setenv("XDG_CONFIG_HOME", "", 1);
  EXPECT_EQ(handler.GetConfigDirectory(), (home_dir / ".config" / "spectrum").string());
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, MigrateLegacyFiles) {
  const std::string playlists =
      R"({"playlists": [{"name": "Old", "songs": [{"url": "https://youtu.be/dQw4w9WgXcQ"}]}]})";
  const std::string settings = R"({"player": {"volume": 35}})";

  WriteLegacyFile("playlists.json", playlists);
  WriteLegacyFile("settings.json", settings);

  // Config directory does not exist yet (as in the first run after updating)
  std::filesystem::remove_all(home_dir / ".config");

  handler.MigrateLegacyFiles();

  EXPECT_EQ(ReadFile(handler.GetPlaylistsPath()), playlists);
  EXPECT_EQ(ReadFile(handler.GetSettingsPath()), settings);
  EXPECT_FALSE(std::filesystem::exists(GetLegacyDirectory() / "playlists.json"));
  EXPECT_FALSE(std::filesystem::exists(GetLegacyDirectory() / "settings.json"));

  // And files moved are the ones parsed
  model::Playlists parsed_playlists;
  ASSERT_TRUE(handler.ParsePlaylists(parsed_playlists));
  ASSERT_THAT(parsed_playlists, SizeIs(1));
  EXPECT_THAT(parsed_playlists[0].name, StrEq("Old"));

  model::Settings parsed_settings;
  ASSERT_TRUE(handler.ParseSettings(parsed_settings));
  EXPECT_EQ(parsed_settings.volume, 35);
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, MigrateLegacyFilesKeepsExistingOnes) {
  const std::string current = R"({"player": {"volume": 80}})";
  const std::string legacy = R"({"player": {"volume": 35}})";

  std::ofstream(handler.GetSettingsPath()) << current;
  WriteLegacyFile("settings.json", legacy);

  handler.MigrateLegacyFiles();

  // File from older version is neither used nor removed
  EXPECT_EQ(ReadFile(handler.GetSettingsPath()), current);
  EXPECT_EQ(ReadFile(GetLegacyDirectory() / "settings.json"), legacy);

  // Nothing happens when there is no file from older version
  std::filesystem::remove_all(GetLegacyDirectory());
  handler.MigrateLegacyFiles();
  EXPECT_EQ(ReadFile(handler.GetSettingsPath()), current);
  EXPECT_FALSE(std::filesystem::exists(handler.GetPlaylistsPath()));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, SaveAndParsePlaylists) {
  const std::string url = "https://www.youtube.com/watch?v=URlPXepBZdo";
  const std::filesystem::path file = home_dir / "song.mp3";
  utils::CreateEmptyFile(file);

  const model::Playlists playlists{
      model::Playlist{
          .name = "coding",
          .songs = {model::Song{.filepath = file, .artist = "cln", .title = "DUST"},
                    model::Song{.artist = "Clipse",
                                .title = "So Be It",
                                .stream_info = model::StreamInfo{.base_url = url}}},
      },
      model::Playlist{.name = "empty", .songs = {}},
  };

  // Directory is created when it does not exist yet
  std::filesystem::remove_all(handler.GetConfigDirectory());
  ASSERT_TRUE(handler.SavePlaylists(playlists));

  model::Playlists parsed;
  ASSERT_TRUE(handler.ParsePlaylists(parsed));
  ASSERT_THAT(parsed, SizeIs(2));

  EXPECT_THAT(parsed[0].name, StrEq("coding"));
  ASSERT_THAT(parsed[0].songs, SizeIs(2));

  // Song from file is saved by its path, as the rest is read from file when it is played
  EXPECT_EQ(parsed[0].songs[0].filepath, file);

  // While song from URL keeps its artist and title, to show them without fetching anything
  ASSERT_TRUE(parsed[0].songs[1].stream_info.has_value());
  EXPECT_THAT(parsed[0].songs[1].stream_info->base_url, StrEq(url));
  EXPECT_THAT(parsed[0].songs[1].artist, StrEq("Clipse"));
  EXPECT_THAT(parsed[0].songs[1].title, StrEq("So Be It"));

  // Playlist without any song is not lost (e.g. all of its files were removed)
  EXPECT_THAT(parsed[1].name, StrEq("empty"));
  EXPECT_THAT(parsed[1].songs, IsEmpty());

  // Nothing was skipped, so there is no backup
  EXPECT_FALSE(std::filesystem::exists(handler.GetPlaylistsPath() + ".bak"));

  // Same when there is no playlist at all (e.g. the last one was deleted)
  ASSERT_TRUE(handler.SavePlaylists(model::Playlists{}));
  ASSERT_TRUE(handler.ParsePlaylists(parsed));

  EXPECT_THAT(parsed, IsEmpty());
  EXPECT_FALSE(std::filesystem::exists(handler.GetPlaylistsPath() + ".bak"));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, CannotSaveWithoutConfigDirectory) {
  WriteLegacyFile("settings.json", R"({"player": {"volume": 35}})");

  // Directory cannot be created, as there is a file using the name of its parent
  const std::filesystem::path parent =
      std::filesystem::path{handler.GetConfigDirectory()}.parent_path();
  std::filesystem::remove_all(parent);
  utils::CreateEmptyFile(parent);

  EXPECT_FALSE(handler.SavePlaylists(model::Playlists{model::Playlist{.name = "coding"}}));
  EXPECT_FALSE(handler.SaveSettings(model::Settings{.volume = 40}));

  // File from older version is kept where it is
  handler.MigrateLegacyFiles();
  EXPECT_TRUE(std::filesystem::exists(GetLegacyDirectory() / "settings.json"));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, DirectoryInPlaceOfFile) {
  // There are directories using the name of both files
  std::filesystem::create_directories(handler.GetPlaylistsPath());
  std::filesystem::create_directories(handler.GetSettingsPath());

  // They cannot be read
  model::Playlists playlists;
  EXPECT_FALSE(handler.ParsePlaylists(playlists));
  EXPECT_THAT(playlists, IsEmpty());

  model::Settings settings;
  EXPECT_FALSE(handler.ParseSettings(settings));
  EXPECT_FALSE(settings.volume.has_value());

  // Neither replaced by a file
  EXPECT_FALSE(handler.SavePlaylists(model::Playlists{model::Playlist{.name = "coding"}}));
  EXPECT_FALSE(handler.SaveSettings(model::Settings{.volume = 40}));

  EXPECT_TRUE(std::filesystem::is_directory(handler.GetPlaylistsPath()));
  EXPECT_TRUE(std::filesystem::is_directory(handler.GetSettingsPath()));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ParseSettingsFileWithUnexpectedStructure) {
  const std::string content = R"([1, 2, 3])";
  std::ofstream(handler.GetSettingsPath()) << content;

  model::Settings settings;
  EXPECT_FALSE(handler.ParseSettings(settings));
  EXPECT_FALSE(settings.volume.has_value());

  // Keep a backup, as file will be overwritten on next save
  EXPECT_THAT(ReadFile(handler.GetSettingsPath() + ".bak"), StrEq(content));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, ParseMalformedFileWhenBackupCannotBeCreated) {
  // There is a directory using the name of backup file
  const std::string backup = handler.GetPlaylistsPath() + ".bak";
  std::filesystem::create_directories(backup);

  WritePlaylistsFile(R"({"playlists": [)");

  model::Playlists playlists;
  EXPECT_FALSE(handler.ParsePlaylists(playlists));
  EXPECT_TRUE(std::filesystem::is_directory(backup));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, MigrateLegacyFilesSkipsWhatCannotBeMoved) {
  const std::string settings = R"({"player": {"volume": 35}})";

  // Only a regular file can be moved
  std::filesystem::create_directories(GetLegacyDirectory() / "playlists.json");
  WriteLegacyFile("settings.json", settings);

  handler.MigrateLegacyFiles();

  EXPECT_FALSE(std::filesystem::exists(handler.GetPlaylistsPath()));
  EXPECT_TRUE(std::filesystem::is_directory(GetLegacyDirectory() / "playlists.json"));

  // And it does not stop the other file from being moved
  EXPECT_EQ(ReadFile(handler.GetSettingsPath()), settings);
  EXPECT_FALSE(std::filesystem::exists(GetLegacyDirectory() / "settings.json"));
}

/* ********************************************************************************************** */

TEST_F(FileHandlerTest, LogPath) {
  EXPECT_EQ(handler.GetLogPath(), (home_dir / ".cache" / "spectrum" / "spectrum.log").string());
}

}  // namespace
