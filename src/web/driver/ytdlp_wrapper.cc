#include "web/driver/ytdlp_wrapper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "nlohmann/json.hpp"
#include "util/formatter.h"
#include "util/process.h"

namespace driver {

namespace {

constexpr double kBitsPerKilobit = 1000;  //!< To convert bit rate from kbps to bps

//! Get value from JSON entry, or fallback if it does not exist, is null or has an unexpected type
template <typename T>
T GetOr(const nlohmann::json& entry, const char* key, const T& fallback) {
  auto it = entry.find(key);
  if (it == entry.end() || it->is_null()) return fallback;

  try {
    return it->get<T>();
  } catch (const nlohmann::json::exception&) {
    return fallback;
  }
}

}  // namespace

//! Split the given input string into artist + title
static void ParseSongTitle(const std::string& input, std::string& artist, std::string& title) {
  static constexpr std::string_view kDelimiter = "-";
  std::string filtered = util::filter_emoji(input);

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

void YtDlpWrapper::Init() {
  // Let user know if YouTube support is available (and which version, as yt-dlp is often updated)
  auto program = util::FindExecutable(std::string{kProgram});

  if (!program) {
    WARN("Cannot find ", kProgram, " in PATH, songs from URL cannot be played");
    return;
  }

  auto result = util::RunProcess({program->string(), "--version"}, kTimeout);
  std::string version = result && result->exit_code == 0 ? util::trim(result->output) : "unknown";

  INFO("Found ", kProgram, "=", program->string(), " version=", version);
}

/* ********************************************************************************************** */

bool YtDlpWrapper::IsAvailable() { return util::FindExecutable(std::string{kProgram}).has_value(); }

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ExtractPlaylist(const std::string& url, std::vector<model::Song>& songs,
                                          const std::atomic<bool>* cancel) {
  auto program = util::FindExecutable(std::string{kProgram});

  if (!program) {
    WARN("Cannot find ", kProgram, " in PATH to extract playlist from URL=", url);
    return error::kStreamFetcherNotFound;
  }

  // Only list entries from playlist (much faster than extracting information from every song)
  INFO("Extract songs from playlist URL=", url);
  std::string output;

  if (error::Code result =
          Run(program->string(), {"--flat-playlist", "--dump-single-json", "--no-warnings"}, url,
              output, cancel);
      result != error::kSuccess) {
    return result;
  }

  nlohmann::json info = nlohmann::json::parse(output, nullptr, /*allow_exceptions=*/false);
  return ParsePlaylist(info, songs);
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ParsePlaylist(const nlohmann::json& info,
                                        std::vector<model::Song>& songs) {
  auto entries = info.is_object() ? info.find("entries") : info.end();

  if (entries == info.end() || !entries->is_array()) {
    ERROR("Could not parse playlist extracted from URL");
    return error::kStreamFetchFailed;
  }

  std::vector<model::Song> parsed;
  int skipped = 0;

  for (const auto& entry : *entries) {
    const std::string id = entry.is_object() ? GetOr<std::string>(entry, "id", "") : "";
    const std::string title = entry.is_object() ? GetOr<std::string>(entry, "title", "") : "";

    // Deleted and private videos are still listed, but they cannot be played
    if (id.empty() || title == "[Deleted video]" || title == "[Private video]") {
      skipped++;
      continue;
    }

    model::Song song{.stream_info =
                         model::StreamInfo{.base_url = "https://www.youtube.com/watch?v=" + id}};
    FillArtistAndTitle(title, entry, song);

    parsed.push_back(std::move(song));
  }

  INFO("Parsed playlist=", std::quoted(GetOr<std::string>(info, "title", "")),
       " with songs=", parsed.size(), " skipped=", skipped);

  songs = std::move(parsed);
  return error::kSuccess;
}

/* ********************************************************************************************** */

void YtDlpWrapper::SetCookiesFromBrowser(const std::string& browser) {
  cookies_browser_ = browser;
  use_cookies_ = false;

  if (!browser.empty()) INFO("Cookies used when requests are refused, from browser=", browser);
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::Run(const std::string& program, const std::vector<std::string>& options,
                              const std::string& url, std::string& output,
                              const std::atomic<bool>* cancel) {
  auto execute = [&](bool with_cookies) {
    std::vector<std::string> args{program};
    args.insert(args.end(), options.begin(), options.end());

    // Browser is a single argument, so program is the only one to make sense of its content
    if (with_cookies) args.insert(args.end(), {std::string{kOptionCookies}, cookies_browser_});

    args.insert(args.end(), {"--", url});
    auto result = util::RunProcess(args, kTimeout, cancel);

    if (result && result->exit_code == 0) {
      output = std::move(result->output);
      return error::kSuccess;
    }

    if (result && result->canceled) {
      LOG("Canceled extracting information from URL=", url);
      return error::kStreamFetchFailed;
    }

    ERROR("Could not extract information from URL=", url, " cookies=", with_cookies,
          result ? (result->timed_out ? ", timed out" : ", error=" + util::trim(result->error))
                 : ", program could not be started");

    // Let user know the reason, instead of only that it failed
    if (!result) return error::kStreamFetchFailed;
    if (result->timed_out) return error::kStreamTimedOut;

    error::Code reason = GetFailureReason(result->error);
    return reason == error::kStreamBlocked && with_cookies ? error::kStreamBlockedWithCookies
                                                           : reason;
  };

  error::Code result = execute(use_cookies_);

  // Cookies are only sent after site refuses a request, and then they are sent by all the next
  // ones (as site would refuse them too)
  if (result == error::kStreamBlocked && !cookies_browser_.empty()) {
    WARN("Requests are being refused, use cookies from browser=", cookies_browser_);
    use_cookies_ = true;
    result = execute(true);
  }

  // Without any cookies to send, there is no reason to keep asking for them
  if (result == error::kStreamCookiesFailed) use_cookies_ = false;

  return result;
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::GetFailureReason(const std::string& error) {
  using std::string_view_literals::operator""sv;

  //! Texts printed by program when site refuses to answer (asking for a login to prove that
  //! request does not come from a bot, which happens after too many requests)
  static constexpr std::array kBlocked{"Sign in to confirm"sv, "HTTP Error 429"sv};

  //! Texts printed by program when video cannot be watched by anyone (or only by its owner)
  static constexpr std::array kUnavailable{"Video unavailable"sv, "Private video"sv,
                                           "has been removed"sv, "is not available"sv,
                                           "does not exist"sv};

  //! Texts printed by program when it cannot read cookies from browser (e.g. browser or profile
  //! does not exist, or it is not a browser known by program)
  static constexpr std::array kCookies{"cookies database"sv, "specified for cookies"sv,
                                       "failed to load cookies"sv};

  auto contains = [&error](const auto& texts) {
    return std::any_of(texts.begin(), texts.end(), [&error](std::string_view text) {
      return error.find(text) != std::string::npos;
    });
  };

  if (contains(kCookies)) return error::kStreamCookiesFailed;
  if (contains(kBlocked)) return error::kStreamBlocked;
  if (contains(kUnavailable)) return error::kStreamUnavailable;

  return error::kStreamFetchFailed;
}

/* ********************************************************************************************** */

void YtDlpWrapper::Finish() {
  // Nothing to clean up, as program is only executed while extracting information
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ExtractInfo(model::Song& song) {
  if (!song.stream_info.has_value() || song.stream_info->base_url.empty()) {
    ERROR("Song does not contain any URL to extract information");
    return error::kStreamFetchFailed;
  }

  // Nothing is fetched while streaming URL from the last time may still be used
  if (Reuse(song)) {
    LOG("Reuse stream info=", *song.stream_info);
    return error::kSuccess;
  }

  // Search for it every time, so it can be installed while application is running
  auto program = util::FindExecutable(std::string{kProgram});

  if (!program) {
    WARN("Cannot find ", kProgram,
         " in PATH to extract information from URL=", song.stream_info->base_url);
    return error::kStreamFetcherNotFound;
  }

  // Extract information as JSON (only for the given video, even if URL contains a playlist)
  const std::string& url = song.stream_info->base_url;
  std::string output;

  if (error::Code result = Run(
          program->string(), {"--dump-single-json", "--no-playlist", "--no-warnings"}, url, output);
      result != error::kSuccess) {
    return result;
  }

  nlohmann::json info = nlohmann::json::parse(output, nullptr, /*allow_exceptions=*/false);
  if (error::Code parsed = ParseInfo(info, song); parsed != error::kSuccess) return parsed;

  LOG("Parsed stream info=", *song.stream_info);
  Keep(song);

  return error::kSuccess;
}

/* ********************************************************************************************** */

bool YtDlpWrapper::Forget(const model::Song& song) {
  if (!song.stream_info.has_value()) return false;

  auto it = songs_kept_.find(song.stream_info->base_url);
  if (it == songs_kept_.end()) return false;

  const bool reused = it->second.reused;
  songs_kept_.erase(it);

  return reused;
}

/* ********************************************************************************************** */

std::optional<std::chrono::seconds> YtDlpWrapper::GetExpiration(const std::string& streaming_url) {
  using std::string_view_literals::operator""sv;

  //! Expiration is informed (as seconds since epoch) either as a parameter or as part of the path
  static constexpr std::array kFields{"expire="sv, "expire/"sv};

  for (const auto& field : kFields) {
    const size_t position = streaming_url.find(field);
    if (position == std::string::npos) continue;

    const size_t begin = position + field.size();
    const size_t end = streaming_url.find_first_not_of("0123456789", begin);
    const std::string seconds = streaming_url.substr(begin, end - begin);

    // Number with more digits than this is not a moment that makes any sense
    static constexpr size_t kMaxDigits = 12;
    if (seconds.empty() || seconds.size() > kMaxDigits) continue;

    return std::chrono::seconds{std::stoll(seconds)};
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

void YtDlpWrapper::Keep(const model::Song& song) {
  const model::StreamInfo& info = *song.stream_info;

  // Without knowing when streaming URL expires, it is safer to always fetch it
  auto expiration = GetExpiration(info.streaming_url);
  if (!expiration) return;

  // Remove what cannot be used anymore
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());

  for (auto it = songs_kept_.begin(); it != songs_kept_.end();) {
    it = it->second.expiration <= now ? songs_kept_.erase(it) : std::next(it);
  }

  // And do not keep information forever, starting from the one that expires first
  if (songs_kept_.size() >= kMaxSongsKept && songs_kept_.find(info.base_url) == songs_kept_.end()) {
    songs_kept_.erase(std::min_element(songs_kept_.begin(), songs_kept_.end(),
                                       [](const auto& lhs, const auto& rhs) {
                                         return lhs.second.expiration < rhs.second.expiration;
                                       }));
  }

  songs_kept_[info.base_url] = SongKept{
      .artist = song.artist,
      .title = song.title,
      .num_channels = song.num_channels,
      .bit_rate = song.bit_rate,
      .duration = song.duration,
      .stream_info = info,
      .expiration = *expiration,
      .reused = false,
  };
}

/* ********************************************************************************************** */

bool YtDlpWrapper::Reuse(model::Song& song) {
  auto it = songs_kept_.find(song.stream_info->base_url);
  if (it == songs_kept_.end()) return false;

  SongKept& kept = it->second;

  // Streaming URL must be valid until the end of song
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch());
  const auto needed = now + std::chrono::seconds{kept.duration} + kExpirationMargin;

  if (kept.expiration <= needed) {
    songs_kept_.erase(it);
    return false;
  }

  song.artist = kept.artist;
  song.title = kept.title;
  song.num_channels = kept.num_channels;
  song.bit_rate = kept.bit_rate;
  song.duration = kept.duration;
  song.stream_info = kept.stream_info;

  kept.reused = true;
  return true;
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ParseInfo(const nlohmann::json& info, model::Song& song) {
  if (!info.is_object()) {
    ERROR("Could not parse information extracted from URL");
    return error::kStreamFetchFailed;
  }

  // Only audio streams are played
  nlohmann::json streams = nlohmann::json::array();

  if (auto formats = info.find("formats"); formats != info.end() && formats->is_array()) {
    for (const auto& format : *formats) {
      if (format.is_object() && GetOr<std::string>(format, "resolution", "") == "audio only") {
        streams.push_back(format);
      }
    }
  }

  const nlohmann::json* entry = SelectStream(streams);

  if (!entry) {
    ERROR("Song has no audio streaming format with URL");
    return error::kStreamFetchFailed;
  }

  // Song may already have an artist guessed from playlist entry (e.g. uploader, as it does not
  // contain any artist), so discard it to use the one from detailed information
  song.artist.clear();
  FillArtistAndTitle(GetOr<std::string>(info, "title", ""), info, song);

  // Duration may be a floating point number (in seconds)
  auto duration = static_cast<uint32_t>(GetOr<double>(info, "duration", 0));
  FillStreamInfo(*entry, duration, song);

  return error::kSuccess;
}

/* ********************************************************************************************** */

void YtDlpWrapper::FillArtistAndTitle(const std::string& title, const nlohmann::json& metadata,
                                      model::Song& song) {
  ParseSongTitle(title, song.artist, song.title);
  if (!song.artist.empty() || !metadata.is_object()) return;

  // Video title has no artist, so use the first valid one from metadata
  for (const char* key : {"artist", "uploader", "channel"}) {
    if (std::string artist = util::trim(util::filter_emoji(GetOr<std::string>(metadata, key, "")));
        !artist.empty()) {
      LOG("Using ", key, " as artist=", std::quoted(artist));
      song.artist = artist;
      return;
    }
  }
}

/* ********************************************************************************************** */

void YtDlpWrapper::FillStreamInfo(const nlohmann::json& entry, uint32_t duration,
                                  model::Song& song) {
  // Any field may be missing or null (e.g. HLS entries have no codec, channels or filesize)
  song.num_channels = GetOr<uint16_t>(entry, "audio_channels", 2);
  song.duration = duration;

  // Audio bit rate is informed in kbps, and it is the only source for it when codec does not
  // inform it in its stream (e.g. Opus)
  if (double bit_rate = GetOr<double>(entry, "abr", 0); bit_rate > 0) {
    song.bit_rate = static_cast<uint32_t>(std::lround(bit_rate * kBitsPerKilobit));
  }

  model::StreamInfo& info = *song.stream_info;

  info.codec = GetOr<std::string>(entry, "acodec", GetOr<std::string>(entry, "protocol", ""));
  info.extension = GetOr<std::string>(entry, "audio_ext", "");
  info.filesize = GetOr<uint64_t>(entry, "filesize", GetOr<uint64_t>(entry, "filesize_approx", 0));
  info.description = GetOr<std::string>(entry, "format", "");
  info.base_url = song.stream_info->base_url;
  info.streaming_url = GetOr<std::string>(entry, "url", "");

  if (auto headers = entry.find("http_headers"); headers != entry.end() && headers->is_object()) {
    for (const auto& [key, value] : headers->items()) {
      if (value.is_string()) info.http_header[key] = value;
    }
  }
}

/* ********************************************************************************************** */

const nlohmann::json* YtDlpWrapper::SelectStream(const nlohmann::json& streams) {
  if (!streams.is_array()) return nullptr;

  // Rank for a single stream, where greater is better (compared field by field)
  auto rank = [](const nlohmann::json& entry) {
    const std::string protocol = GetOr<std::string>(entry, "protocol", "");
    const std::string format_id = GetOr<std::string>(entry, "format_id", "");

    bool direct = protocol == "https" || protocol == "http";
    int language = GetOr<int>(entry, "language_preference", 0);
    bool drc = GetOr<bool>(entry, "has_drc", false) || format_id.find("drc") != std::string::npos;

    return std::make_tuple(direct, language, !drc, GetOr<double>(entry, "quality", 0),
                           GetOr<double>(entry, "abr", 0));
  };

  const nlohmann::json* selected = nullptr;

  for (const auto& entry : streams) {
    if (!entry.is_object() || GetOr<std::string>(entry, "url", "").empty()) continue;

    if (!selected || rank(entry) > rank(*selected)) selected = &entry;
  }

  if (selected) INFO("Selected stream format=", GetOr<std::string>(*selected, "format", ""));

  return selected;
}

}  // namespace driver
