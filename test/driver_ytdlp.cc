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

}  // namespace
