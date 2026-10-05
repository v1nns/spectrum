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
#include <vector>

#include "general/utils.h"
#include "model/player_status.h"
#include "model/playlist.h"
#include "model/remote_command.h"
#include "model/settings.h"
#include "model/song.h"
#include "model/stream_info.h"
#include "util/file_handler.h"
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
  EXPECT_THAT(model::GetRemoteCommandNames(), ::testing::EndsWith(", shuffle, status, subscribe"));
}

/* ********************************************************************************************** */

TEST(PlayerStatusTest, ConvertToJson) {
  // Nothing is playing
  EXPECT_THAT(model::to_json(model::PlayerStatus{}),
              StrEq(R"({"artist":"","duration":0,"muted":false,"position":0,"repeat":"off",)"
                    R"("shuffle":false,"state":"stopped","title":"","volume":100})"));

  model::PlayerStatus status{
      .state = model::Song::MediaState::Play,
      .artist = "Deko \"Tok\"",
      .title = "First line\nSecond line",
      .position = 75,
      .duration = 3725,
      .volume = model::Volume{0.35F},
      .repeat = model::RepeatMode::All,
      .shuffle = true,
  };
  status.volume.ToggleMute();

  // Always a single line, no matter the content
  EXPECT_THAT(
      model::to_json(status),
      StrEq(R"({"artist":"Deko \"Tok\"","duration":3725,"muted":true,"position":75,"repeat":"all",)"
            R"("shuffle":true,"state":"playing","title":"First line\nSecond line","volume":35})"));

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
  std::string content(util::kMaxRemoteRequestSize * 4, 'a');

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
