#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "model/song.h"
#include "nlohmann/json.hpp"
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

  // Skip fields that are empty, have unexpected type, or contain only non-ASCII characters
  song = FillArtistAndTitle(
      title, R"({"artist": ["Elevation"], "uploader": " \u30a8 ", "channel": "Worship"})");
  EXPECT_THAT(song.artist, StrEq("Worship"));
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

}  // namespace
