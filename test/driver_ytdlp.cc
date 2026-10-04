#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "model/application_error.h"
#include "model/song.h"
#include "model/stream_info.h"
#include "nlohmann/json.hpp"
#include "util/url.h"
#include "web/driver/ytdlp_wrapper.h"

namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::StrEq;

/**
 * @brief Tests with YtDlpWrapper class (only parsing, without fetching anything from network)
 */
class YtDlpWrapperTest : public ::testing::Test {
 protected:
  //! Select stream from list and return its format identifier (or empty, if none was selected)
  static std::string SelectFormatId(const nlohmann::json& streams) {
    const nlohmann::json* selected = driver::YtDlpWrapper::SelectStream(streams);
    return selected ? selected->at("format_id").get<std::string>() : "";
  }

  //! Fill song with artist and title from the given video title and metadata
  static model::Song FillArtistAndTitle(const std::string& title, const std::string& metadata) {
    model::Song song;
    driver::YtDlpWrapper::FillArtistAndTitle(
        title, nlohmann::json::parse(metadata, nullptr, /*allow_exceptions=*/false), song);
    return song;
  }

  //! Parse information extracted by yt-dlp (as JSON) into song
  error::Code ParseInfo(const std::string& info, model::Song& song) {
    return wrapper.ParseInfo(nlohmann::json::parse(info, nullptr, /*allow_exceptions=*/false),
                             song);
  }

  //! Parse playlist extracted by yt-dlp (as JSON) into list of songs
  static error::Code ParsePlaylist(const std::string& info, std::vector<model::Song>& songs) {
    return driver::YtDlpWrapper::ParsePlaylist(
        nlohmann::json::parse(info, nullptr, /*allow_exceptions=*/false), songs);
  }

  //! Fill song with streaming information from the given entry
  void FillStreamInfo(const nlohmann::json& entry, model::Song& song) {
    wrapper.FillStreamInfo(entry, /*duration=*/212, song);
  }

  driver::YtDlpWrapper wrapper;  //!< Class under test (python is never initialized here)
};

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, SelectBestStream) {
  // Based on formats from a real video, plus dubbed and DRC variants
  auto streams = nlohmann::json::parse(R"([
    {"format_id": "139", "url": "u", "protocol": "https", "language_preference": 10,
     "quality": 2, "abr": 48.8},
    {"format_id": "251", "url": "u", "protocol": "https", "language_preference": 10,
     "quality": 3, "abr": 128.9},
    {"format_id": "140-drc", "url": "u", "protocol": "https", "language_preference": 10,
     "has_drc": true, "quality": 3, "abr": 130.1},
    {"format_id": "140-dubbed", "url": "u", "protocol": "https", "language_preference": -1,
     "quality": 3, "abr": 131.0},
    {"format_id": "234", "url": "u", "protocol": "m3u8_native", "language_preference": 10,
     "quality": 5, "abr": null},
    {"format_id": "140", "url": "u", "protocol": "https", "language_preference": 10,
     "quality": 3, "abr": 129.5}
  ])");

  // Direct stream, original language, without DRC and highest bitrate
  EXPECT_THAT(SelectFormatId(streams), StrEq("140"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, SelectHlsStreamWhenThereIsNoOther) {
  auto streams = nlohmann::json::parse(R"([
    {"format_id": "233", "url": "u", "protocol": "m3u8_native", "quality": -1},
    {"format_id": "234", "url": "u", "protocol": "m3u8_native", "quality": 1}
  ])");

  EXPECT_THAT(SelectFormatId(streams), StrEq("234"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, SelectNothingWithoutUrl) {
  auto streams = nlohmann::json::parse(R"([
    {"format_id": "140", "protocol": "https", "quality": 3},
    {"format_id": "251", "url": null, "protocol": "https", "quality": 3},
    "not an object"
  ])");

  EXPECT_THAT(SelectFormatId(streams), IsEmpty());
  EXPECT_THAT(SelectFormatId(nlohmann::json::object()), IsEmpty());
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, FillStreamInfoWithMissingFields) {
  // HLS entries have most of their fields as null
  auto entry = nlohmann::json::parse(R"({
    "format_id": "234", "url": "https://stream", "protocol": "m3u8_native", "acodec": null,
    "audio_ext": null, "audio_channels": null, "filesize": null, "format": "234 - audio only",
    "http_headers": {"User-Agent": "dummy", "Invalid": 1}
  })");

  model::Song song{.stream_info = model::StreamInfo{.base_url = "https://youtu.be/dQw4w9WgXcQ"}};
  FillStreamInfo(entry, song);

  ASSERT_TRUE(song.stream_info.has_value());
  EXPECT_THAT(song.num_channels, Eq(2));
  EXPECT_THAT(song.duration, Eq(212));
  EXPECT_THAT(song.stream_info->codec, StrEq("m3u8_native"));
  EXPECT_THAT(song.stream_info->extension, IsEmpty());
  EXPECT_THAT(song.stream_info->filesize, Eq(0));
  EXPECT_THAT(song.stream_info->streaming_url, StrEq("https://stream"));
  EXPECT_THAT(song.stream_info->base_url, StrEq("https://youtu.be/dQw4w9WgXcQ"));
  EXPECT_THAT(song.stream_info->http_header.size(), Eq(1));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, ArtistFromVideoTitle) {
  auto song =
      FillArtistAndTitle("Clipse - So Be It (Official Music Video)",
                         R"({"artist": null, "uploader": "Clipse TV", "channel": "Clipse"})");

  // Artist from video title has priority over metadata
  EXPECT_THAT(song.artist, StrEq("Clipse"));
  EXPECT_THAT(song.title, StrEq("So Be It (Official Music Video)"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, ArtistFromMetadataWhenVideoTitleHasNone) {
  const std::string title{"SO BE IT | Elevation Worship (feat. Tiffany Hudson & Chris Brown)"};

  // Based on metadata from a real video (it has no artist)
  auto song = FillArtistAndTitle(
      title,
      R"({"artist": null, "uploader": "Elevation Worship", "channel": "Elevation Worship"})");

  EXPECT_THAT(song.artist, StrEq("Elevation Worship"));
  EXPECT_THAT(song.title, StrEq(title));

  // Artist field has priority over uploader and channel
  song = FillArtistAndTitle(title, R"({"artist": "Elevation", "uploader": "Elevation Worship"})");
  EXPECT_THAT(song.artist, StrEq("Elevation"));

  // Skip fields that are empty, have unexpected type, or contain only emojis
  song = FillArtistAndTitle(
      title, R"({"artist": ["Elevation"], "uploader": " \ud83c\udfb5 ", "channel": "Worship"})");
  EXPECT_THAT(song.artist, StrEq("Worship"));

  // Characters out of ASCII are kept
  song = FillArtistAndTitle(title, R"({"artist": null, "uploader": " \u30a8 "})");
  EXPECT_THAT(song.artist, StrEq("\u30a8"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, KeepNonAsciiTitleWithoutEmoji) {
  // Based on metadata from a real video (title has only cyrillic characters), plus an emoji
  const std::string title{"меланхолия"};
  auto song = FillArtistAndTitle(title + " \U0001F3B5",
                                 R"({"artist": "vecher 1998", "uploader": "vecher 1998 - Topic"})");

  EXPECT_THAT(song.artist, StrEq("vecher 1998"));
  EXPECT_THAT(song.title, StrEq(title));

  // Invalid UTF-8 bytes are removed
  song = FillArtistAndTitle("caf\xC3\xA9 \xFF\xC3", "");
  EXPECT_THAT(song.title, StrEq("caf\u00e9"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, ParseInfoReplacesArtistFromPlaylistEntry) {
  const std::string info = R"json({
    "title": "меланхолия",
    "artist": "vecher 1998", "uploader": "vecher 1998 - Topic",
    "formats": [{"format_id": "251", "url": "https://best", "protocol": "https",
                 "resolution": "audio only"}]
  })json";

  // Playlist entry does not contain artist, so uploader was used when it was imported
  model::Song song{.artist = "vecher 1998 - Topic",
                   .stream_info = model::StreamInfo{.base_url = "https://youtu.be/84vL55y8fog"}};
  ASSERT_EQ(ParseInfo(info, song), error::kSuccess);

  EXPECT_THAT(song.artist, StrEq("vecher 1998"));
  EXPECT_THAT(song.title, StrEq("меланхолия"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, NoArtistWithoutMetadata) {
  auto song =
      FillArtistAndTitle("so be it", R"({"artist": null, "uploader": "", "channel": null})");
  EXPECT_THAT(song.artist, IsEmpty());
  EXPECT_THAT(song.title, StrEq("so be it"));

  // Invalid JSON (e.g. missing variable from python snippet)
  song = FillArtistAndTitle("so be it", "");
  EXPECT_THAT(song.artist, IsEmpty());
  EXPECT_THAT(song.title, StrEq("so be it"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, ParseInfoFromYtDlp) {
  // Based on output from "yt-dlp --dump-single-json" (only relevant fields)
  const std::string info = R"json({
    "title": "Clipse - So Be It (Official Music Video)", "duration": 212.6, "uploader": "Clipse",
    "formats": [
      {"format_id": "18", "url": "https://video", "protocol": "https", "resolution": "640x360"},
      {"format_id": "139", "url": "https://low", "protocol": "https", "resolution": "audio only",
       "quality": 2, "abr": 48.8, "acodec": "mp4a.40.5"},
      {"format_id": "251", "url": "https://best", "protocol": "https", "resolution": "audio only",
       "quality": 3, "abr": 128.9, "acodec": "opus", "audio_channels": 2}
    ]
  })json";

  model::Song song{.stream_info = model::StreamInfo{.base_url = "https://youtu.be/URlPXepBZdo"}};
  ASSERT_EQ(ParseInfo(info, song), error::kSuccess);

  EXPECT_THAT(song.artist, StrEq("Clipse"));
  EXPECT_THAT(song.title, StrEq("So Be It (Official Music Video)"));
  EXPECT_THAT(song.duration, Eq(212));
  ASSERT_TRUE(song.stream_info.has_value());
  EXPECT_THAT(song.stream_info->streaming_url, StrEq("https://best"));
  EXPECT_THAT(song.stream_info->codec, StrEq("opus"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, ParseInfoWithoutAudioStream) {
  model::Song song{.stream_info = model::StreamInfo{.base_url = "https://youtu.be/URlPXepBZdo"}};

  // Only video formats
  EXPECT_EQ(
      ParseInfo(R"({"title": "Video", "formats": [{"url": "u", "resolution": "640x360"}]})", song),
      error::kStreamFetchFailed);

  // Invalid output
  EXPECT_EQ(ParseInfo("ERROR: Unsupported URL", song), error::kStreamFetchFailed);
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, IdentifyPlaylistUrl) {
  EXPECT_TRUE(util::IsYoutubePlaylistUrl("https://www.youtube.com/playlist?list=PLabc-123_x"));
  EXPECT_TRUE(util::IsYoutubePlaylistUrl("youtube.com/playlist?list=PLabc"));

  // Video that belongs to a playlist (or mix) imports the whole playlist
  EXPECT_TRUE(util::IsYoutubePlaylistUrl(
      "https://www.youtube.com/watch?v=dQw4w9WgXcQ&list=RDdQw4w9WgXcQ&start_radio=1"));
  EXPECT_TRUE(util::IsYoutubePlaylistUrl("https://youtu.be/dQw4w9WgXcQ?list=PLabc"));

  // Single video
  EXPECT_FALSE(util::IsYoutubePlaylistUrl("https://www.youtube.com/watch?v=dQw4w9WgXcQ"));
  EXPECT_FALSE(util::IsYoutubePlaylistUrl("https://youtu.be/dQw4w9WgXcQ?t=42"));

  // Not from YouTube
  EXPECT_FALSE(util::IsYoutubePlaylistUrl("https://example.com/playlist?list=PLabc"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpWrapperTest, ParsePlaylistFromYtDlp) {
  // Based on output from "yt-dlp --flat-playlist --dump-single-json" (only relevant fields)
  const std::string info = R"json({
    "_type": "playlist", "title": "Worship",
    "entries": [
      {"id": "edZVnKxKEUU", "title": "SO BE IT | Elevation Worship", "channel": "Elevation Worship"},
      {"id": "URlPXepBZdo", "title": "Clipse - So Be It (Official Music Video)"},
      {"id": "xxxxxxxxxxx", "title": "[Deleted video]"},
      {"id": "yyyyyyyyyyy", "title": "[Private video]"},
      {"title": "No identifier"}
    ]
  })json";

  std::vector<model::Song> songs;
  ASSERT_EQ(ParsePlaylist(info, songs), error::kSuccess);
  ASSERT_THAT(songs.size(), Eq(2));

  ASSERT_TRUE(songs[0].stream_info.has_value());
  EXPECT_THAT(songs[0].stream_info->base_url, StrEq("https://www.youtube.com/watch?v=edZVnKxKEUU"));
  EXPECT_THAT(songs[0].artist, StrEq("Elevation Worship"));
  EXPECT_THAT(songs[0].title, StrEq("SO BE IT | Elevation Worship"));

  EXPECT_THAT(songs[1].stream_info->base_url, StrEq("https://www.youtube.com/watch?v=URlPXepBZdo"));
  EXPECT_THAT(songs[1].artist, StrEq("Clipse"));
  EXPECT_THAT(songs[1].title, StrEq("So Be It (Official Music Video)"));

  // Output that does not contain a playlist
  EXPECT_EQ(ParsePlaylist(R"({"title": "Single video"})", songs), error::kStreamFetchFailed);
}

/* ********************************************************************************************** */

/**
 * @brief Tests with YtDlpWrapper class running an external program: a script named as yt-dlp,
 * which is the only program found in PATH (so nothing is fetched from network)
 */
class YtDlpProgramTest : public ::testing::Test {
 protected:
  //! URL from song used in tests
  static constexpr const char* kSongUrl = "https://youtu.be/URlPXepBZdo";

  //! URL from playlist used in tests
  static constexpr const char* kPlaylistUrl = "https://www.youtube.com/playlist?list=PL123";

  void SetUp() override {
    if (const char* path = std::getenv("PATH"); path) original_path = path;

    dir = std::filesystem::temp_directory_path() / "spectrum_ytdlp_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    setenv("PATH", dir.c_str(), 1);
  }

  void TearDown() override {
    if (original_path.has_value()) {
      setenv("PATH", original_path->c_str(), 1);
    } else {
      unsetenv("PATH");
    }

    std::filesystem::remove_all(dir);
  }

  //! Create script to be executed as yt-dlp (only shell builtins are available, as PATH is changed)
  void InstallProgram(const std::string& script) const {
    const std::filesystem::path program = dir / "yt-dlp";

    std::ofstream(program) << "#!/bin/sh\n" << script << "\n";
    std::filesystem::permissions(program, std::filesystem::perms::owner_all);
  }

  //! Create song with only its URL, as it is before extracting information
  static model::Song CreateSong() {
    return model::Song{.stream_info = model::StreamInfo{.base_url = kSongUrl}};
  }

  std::optional<std::string> original_path;  //!< PATH before test
  std::filesystem::path dir;                 //!< Temporary directory, the only one in PATH
  driver::YtDlpWrapper wrapper;              //!< Class under test
};

/* ********************************************************************************************** */

TEST_F(YtDlpProgramTest, ProgramNotFound) {
  EXPECT_FALSE(driver::YtDlpWrapper::IsAvailable());

  // Nothing happens, it is only informed in log
  wrapper.Init();

  model::Song song = CreateSong();
  EXPECT_EQ(wrapper.ExtractInfo(song), error::kStreamFetcherNotFound);

  std::vector<model::Song> songs;
  EXPECT_EQ(driver::YtDlpWrapper::ExtractPlaylist(kPlaylistUrl, songs),
            error::kStreamFetcherNotFound);
  EXPECT_THAT(songs, IsEmpty());

  // File that cannot be executed is not used
  std::ofstream(dir / "yt-dlp") << "#!/bin/sh\n";
  EXPECT_FALSE(driver::YtDlpWrapper::IsAvailable());
}

/* ********************************************************************************************** */

TEST_F(YtDlpProgramTest, ProgramFound) {
  InstallProgram(R"sh([ "$1" = "--version" ] && echo 2026.08.19)sh");
  EXPECT_TRUE(driver::YtDlpWrapper::IsAvailable());

  // Version is only informed in log, even when it cannot be read
  wrapper.Init();

  InstallProgram("exit 1");
  wrapper.Init();

  wrapper.Finish();
}

/* ********************************************************************************************** */

TEST_F(YtDlpProgramTest, ExtractInfo) {
  // Information is printed only when program is executed with the expected arguments
  InstallProgram(R"sh(
[ "$1" = "--dump-single-json" ] && [ "$2" = "--no-playlist" ] || exit 2
[ "$5" = "https://youtu.be/URlPXepBZdo" ] || exit 3

printf '%s' '{
  "title": "Clipse - So Be It (Official Music Video)", "duration": 212.6,
  "formats": [
    {"format_id": "18", "url": "https://video", "protocol": "https", "resolution": "640x360"},
    {"format_id": "251", "url": "https://best", "protocol": "https", "resolution": "audio only",
     "quality": 3, "abr": 128.9, "acodec": "opus", "audio_channels": 2}
  ]
}')sh");

  model::Song song = CreateSong();
  ASSERT_EQ(wrapper.ExtractInfo(song), error::kSuccess);

  EXPECT_THAT(song.artist, StrEq("Clipse"));
  EXPECT_THAT(song.title, StrEq("So Be It (Official Music Video)"));
  EXPECT_THAT(song.duration, Eq(212));

  ASSERT_TRUE(song.stream_info.has_value());
  EXPECT_THAT(song.stream_info->base_url, StrEq(kSongUrl));
  EXPECT_THAT(song.stream_info->streaming_url, StrEq("https://best"));
  EXPECT_THAT(song.stream_info->codec, StrEq("opus"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpProgramTest, ExtractInfoFails) {
  // Song without any URL, program is not even executed
  model::Song empty;
  EXPECT_EQ(wrapper.ExtractInfo(empty), error::kStreamFetchFailed);

  empty.stream_info = model::StreamInfo{};
  EXPECT_EQ(wrapper.ExtractInfo(empty), error::kStreamFetchFailed);

  // Program fails (e.g. video is not available)
  InstallProgram(R"sh(echo "ERROR: Video unavailable" >&2; exit 1)sh");

  model::Song song = CreateSong();
  EXPECT_EQ(wrapper.ExtractInfo(song), error::kStreamFetchFailed);

  // Program prints something that is not the expected information
  InstallProgram("echo unexpected");
  EXPECT_EQ(wrapper.ExtractInfo(song), error::kStreamFetchFailed);

  // Video without any audio stream
  InstallProgram(R"sh(printf '%s' '{"title": "Silent", "formats": []}')sh");
  EXPECT_NE(wrapper.ExtractInfo(song), error::kSuccess);

  // Song is not filled by any of them
  EXPECT_THAT(song.stream_info->streaming_url, IsEmpty());
}

/* ********************************************************************************************** */

TEST_F(YtDlpProgramTest, ExtractPlaylist) {
  // Playlist is printed only when program is executed with the expected arguments
  InstallProgram(R"sh(
[ "$1" = "--flat-playlist" ] && [ "$2" = "--dump-single-json" ] || exit 2
[ "$5" = "https://www.youtube.com/playlist?list=PL123" ] || exit 3

printf '%s' '{
  "title": "So be it",
  "entries": [
    {"id": "URlPXepBZdo", "title": "Clipse - So Be It (Official Music Video)"},
    {"id": "deleted", "title": "[Deleted video]"},
    {"id": "84vL55y8fog", "title": "меланхолия", "uploader": "vecher 1998"}
  ]
}')sh");

  std::vector<model::Song> songs;
  ASSERT_EQ(driver::YtDlpWrapper::ExtractPlaylist(kPlaylistUrl, songs), error::kSuccess);
  ASSERT_THAT(songs.size(), Eq(2));

  EXPECT_THAT(songs[0].stream_info->base_url, StrEq("https://www.youtube.com/watch?v=URlPXepBZdo"));
  EXPECT_THAT(songs[0].artist, StrEq("Clipse"));
  EXPECT_THAT(songs[0].title, StrEq("So Be It (Official Music Video)"));

  EXPECT_THAT(songs[1].stream_info->base_url, StrEq("https://www.youtube.com/watch?v=84vL55y8fog"));
  EXPECT_THAT(songs[1].artist, StrEq("vecher 1998"));
  EXPECT_THAT(songs[1].title, StrEq("меланхолия"));
}

/* ********************************************************************************************** */

TEST_F(YtDlpProgramTest, ExtractPlaylistFails) {
  std::vector<model::Song> songs{CreateSong()};

  // Program fails (e.g. playlist is private)
  InstallProgram(R"sh(echo "ERROR: The playlist does not exist" >&2; exit 1)sh");
  EXPECT_EQ(driver::YtDlpWrapper::ExtractPlaylist(kPlaylistUrl, songs), error::kStreamFetchFailed);

  // Program prints something that is not a playlist
  InstallProgram("echo unexpected");
  EXPECT_EQ(driver::YtDlpWrapper::ExtractPlaylist(kPlaylistUrl, songs), error::kStreamFetchFailed);

  // User cancels it while program is still running
  InstallProgram("exec /bin/sleep 5");
  const std::atomic<bool> cancel = true;

  const auto start = std::chrono::steady_clock::now();
  EXPECT_EQ(driver::YtDlpWrapper::ExtractPlaylist(kPlaylistUrl, songs, &cancel),
            error::kStreamFetchFailed);
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(2));

  // List of songs is not changed by any of them
  EXPECT_THAT(songs.size(), Eq(1));
}

}  // namespace
