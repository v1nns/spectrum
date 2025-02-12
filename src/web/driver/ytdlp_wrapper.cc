#include "web/driver/ytdlp_wrapper.h"

#include <regex>

#include "nlohmann/json.hpp"
#include "util/formatter.h"
#include "util/logger.h"

namespace driver {

//! Split the given input string into artist + title
static void ParseSongTitle(const std::string& input, std::string& artist, std::string& title) {
  static constexpr std::string_view kDelimiter = "-";

  // Find the occurrences of the delimiter in the input string
  size_t first_pos = input.find(kDelimiter);
  size_t second_pos = input.find(kDelimiter, first_pos + kDelimiter.length());

  if (first_pos == std::string::npos || second_pos != std::string::npos) {
    // If the delimiter appears more than once or not at all, only fill the title
    title = util::trim(input);
  } else {
    // Split the string into artist + title
    artist = util::trim(input.substr(0, first_pos));
    title = util::trim(input.substr(first_pos + kDelimiter.length()));
  }
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ExtractInfo(model::Song& song) {
  if (!song.stream_info.has_value() || song.stream_info->base_url.empty()) {
    ERROR("Song does not contain any URL to extract information");
    return error::kUnknownError;
  }

  std::string program = std::regex_replace(kExtractInfo.data(), std::regex("###"),
                                           song.stream_info->base_url.c_str());

  PythonWrapper python;
  python.Run(program);

  // Get extracted info from URL
  std::string title = python.GetString(kAudioTitle);
  uint32_t duration = python.GetLong(kAudioDuration);
  std::string raw_streams = python.GetString(kStreamInfo.data());

  // Parse into JSON
  nlohmann::json streams = nlohmann::json::parse(raw_streams);

  if (streams.empty()) {
    ERROR("Song has no valid streaming format");
    // TODO: improve error
    return error::kUnknownError;
  }

  // Always get first entry (maybe change logic to prioritize m4a)
  nlohmann::json entry = streams.items().begin().value();

  ParseSongTitle(title, song.artist, song.title);
  song.num_channels = entry["audio_channels"];
  song.duration = duration;

  model::StreamInfo& info = *song.stream_info;

  info.codec = entry["acodec"];
  info.extension = entry["audio_ext"];
  info.filesize = entry["filesize"];
  info.description = entry["format"];
  info.base_url = song.stream_info->base_url;
  info.streaming_url = entry["url"];

  for (const auto& [key, value] : entry["http_headers"].items()) {
    info.http_header[key] = value;
  }

  LOG("Parsed stream info=", *song.stream_info);
  return error::kSuccess;
}

}  // namespace driver
