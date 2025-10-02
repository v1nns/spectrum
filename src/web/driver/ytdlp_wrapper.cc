#include "web/driver/ytdlp_wrapper.h"

#include <regex>

#include "nlohmann/json.hpp"
#include "util/formatter.h"

namespace driver {

//! Split the given input string into artist + title
static void ParseSongTitle(const std::string& input, std::string& artist, std::string& title) {
  static constexpr std::string_view kDelimiter = "-";
  std::string filtered = util::filter_ascii(input);

  // Find the occurrences of the delimiter in the input string
  size_t first_pos = filtered.find(kDelimiter);
  size_t second_pos = filtered.find(kDelimiter, first_pos + kDelimiter.length());

  if (first_pos == std::string::npos || second_pos != std::string::npos) {
    // If the delimiter appears more than once or not at all, only fill the title
    title = util::trim(filtered);
  } else {
    // Split the string into artist + title
    artist = util::trim(filtered.substr(0, first_pos));
    title = util::trim(filtered.substr(first_pos + kDelimiter.length()));
  }
}

/* ********************************************************************************************** */

void YtDlpWrapper::Init() { python_.Init(); }

/* ********************************************************************************************** */

void YtDlpWrapper::Finish() { python_.Finish(); }

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ExtractInfo(model::Song& song) {
  if (!song.stream_info.has_value() || song.stream_info->base_url.empty()) {
    ERROR("Song does not contain any URL to extract information");
    return error::kUnknownError;
  }

  std::string program = std::regex_replace(kExtractInfo.data(), std::regex("###"),
                                           song.stream_info->base_url.c_str());

  if (bool result = python_.Run(program); !result || !python_.GetBool(kStreamFound)) {
    ERROR("Could not fetch streaming format from URL=", song.stream_info->base_url);
    return error::kUnknownError;
  }

  // Get extracted info from URL
  std::string title = python_.GetString(kAudioTitle);
  uint32_t duration = python_.GetLong(kAudioDuration);
  std::string raw_streams = python_.GetString(kStreamInfo.data());

  // Parse into JSON
  nlohmann::json streams = nlohmann::json::parse(raw_streams, nullptr, /*allow_exceptions=*/false);

  if (streams.empty()) {
    ERROR("Song has no valid streaming format");
    return error::kUnknownError;
  }

  // Always get first entry (TODO: maybe change logic to prioritize m4a)
  nlohmann::json entry = streams.items().begin().value();

  ParseSongTitle(title, song.artist, song.title);

  FillStreamInfo(entry, duration, song);

  LOG("Parsed stream info=", *song.stream_info);
  return error::kSuccess;
}

/* ********************************************************************************************** */

void YtDlpWrapper::FillStreamInfo(const nlohmann::json& entry, uint32_t duration,
                                  model::Song& song) {
  song.num_channels =
      entry.contains("audio_channels") ? entry["audio_channels"].template get<int>() : 2;
  song.duration = duration;

  model::StreamInfo& info = *song.stream_info;

  info.codec = entry.contains("acodec") ? entry["acodec"] : entry["protocol"];
  info.extension = entry["audio_ext"];
  info.filesize = entry.contains("filesize") ? entry["filesize"].template get<int>() : 0;
  info.description = entry["format"];
  info.base_url = song.stream_info->base_url;
  info.streaming_url = entry["url"];

  for (const auto& [key, value] : entry["http_headers"].items()) {
    info.http_header[key] = value;
  }
}

}  // namespace driver
