#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>

#include "general/utils.h"
#include "util/file_handler.h"
#include "util/process.h"
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

    home_dir = std::filesystem::temp_directory_path() / "spectrum_file_handler_test";
    std::filesystem::remove_all(home_dir);
    std::filesystem::create_directories(home_dir / ".config" / "spectrum");
    setenv("HOME", home_dir.c_str(), 1);

    // Otherwise files would be saved in the real directory from user
    unsetenv("XDG_CONFIG_HOME");
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

}  // namespace
