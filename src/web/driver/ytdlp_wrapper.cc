#include "web/driver/ytdlp_wrapper.h"

#include <regex>

#include "nlohmann/json.hpp"
#include "util/logger.h"

namespace driver {

YtDlpWrapper::YtDlpWrapper() {}

/* ********************************************************************************************** */

YtDlpWrapper::~YtDlpWrapper() {}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ExtractInfo(const std::string& url, model::Song& output) {
  std::string program = std::regex_replace(kExtractInfo.data(), std::regex("###"), url.c_str());

  PythonWrapper python;
  python.Run(program);

  // Get extracted info from URL
  std::string title = python.GetString(kAudioTitle);
  uint32_t duration = python.GetLong(kAudioDuration);
  std::string raw_streams = python.GetString(kStreamInfo.data());

  // Parse into JSON
  nlohmann::json streams = nlohmann::json::parse(raw_streams);

  if (streams.empty()) {
    // TODO: improve error
    return error::kUnknownError;
  }

  // Always get first entry (maybe change logic to prioritize m4a)
  nlohmann::json entry = streams.items().begin().value();

  // TODO: create logic to split string by '-' and fill artist + title
  output.title = title;
  output.num_channels = entry["audio_channels"];
  output.duration = duration;

  model::StreamInfo info{
      .codec = entry["acodec"],
      .extension = entry["audio_ext"],
      .filesize = entry["filesize"],
      .description = entry["format"],
      .base_url = url,
      .streaming_url = entry["url"],
  };

  for (const auto& [key, value] : entry["http_headers"].items()) {
    info.http_header[key] = value;
  }

  output.stream_info = std::move(info);

  LOG("Parsed stream info=", *output.stream_info);
  return error::kSuccess;
}

}  // namespace driver
