#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "general/utils.h"
#include "util/file_handler.h"

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

    home_dir = std::filesystem::temp_directory_path() / "spectrum_file_handler_test";
    std::filesystem::create_directories(home_dir / ".cache" / "spectrum");
    setenv("HOME", home_dir.c_str(), 1);
  }

  void TearDown() override {
    if (original_home.has_value()) {
      setenv("HOME", original_home->c_str(), 1);
    } else {
      unsetenv("HOME");
    }

    std::filesystem::remove_all(home_dir);
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

  std::optional<std::string> original_home;  //!< HOME before test
  std::filesystem::path home_dir;            //!< Temporary HOME directory
  util::FileHandler handler;                 //!< Class under test
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

}  // namespace
