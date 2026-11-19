#include "web/driver/ytdlp_wrapper.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"
#include "util/formatter.h"
#include "util/process.h"
#include "util/url.h"

namespace driver {

namespace {

constexpr double kBitsPerKilobit = 1000;  //!< To convert bit rate from kbps to bps

//! Fields from metadata containing who uploaded the video
constexpr std::array<const char*, 2> kUploaderKeys{"uploader", "channel"};

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

//! Remove suffix added by YouTube to uploader name, e.g. "Artist - Topic" or "ArtistVEVO"
std::string CleanUploader(const std::string& name) {
  static const std::regex kSuffix(R"((\s+-\s+Topic|VEVO)$)");
  return util::trim(std::regex_replace(name, kSuffix, ""));
}

//! Remove quotes around the whole text (ASCII or typographic ones), returning true if removed
bool Unquote(std::string& text) {
  static const std::regex kQuoted(R"(^\s*(?:"|“)(.+)(?:"|”)\s*$)");

  std::smatch match;
  if (!std::regex_match(text, match, kQuoted)) return false;

  text = util::trim(match.str(1));
  return true;
}

//! Compare text ignoring case (only ASCII letters)
bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs) {
  return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
  });
}

}  // namespace

//! Split the given input string into artist + title
static void ParseSongTitle(const std::string& input, std::string& artist, std::string& title) {
  // Separator surrounded by spaces (so hyphens from names like "Jay-Z" are not taken as one):
  // hyphen, en dash, em dash or bullet
  static const std::regex kSeparator(R"(\s+(-|–|—|•)\s+)");
  static constexpr std::string_view kDelimiter = "-";
  std::string filtered = util::filter_emoji(input);

  // Split by the first separator, as title may contain more sections (e.g. "Song | Genre"), which
  // are cleaned when searching for lyrics. But artist is never after one of these sections
  const std::string head = filtered.substr(0, filtered.find('|'));

  if (std::smatch match; std::regex_search(head, match, kSeparator)) {
    artist = util::trim(match.prefix());
    title = util::trim(filtered.substr(static_cast<size_t>(match.position() + match.length())));
    return;
  }

  // Otherwise, find the occurrences of the delimiter in the input string
  size_t first_pos = head.find(kDelimiter);
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
  INFO("Extract songs from playlist URL=", url);
  const int max_entries = util::IsYoutubeMixUrl(url) ? kMixMaxSongs : 0;

  return ListEntries(
      url, max_entries, [&songs](const nlohmann::json& info) { return ParsePlaylist(info, songs); },
      cancel);
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::SearchArtists(const std::string& query,
                                        std::vector<model::YoutubeChannel>& channels,
                                        const std::atomic<bool>* cancel) {
  INFO("Search channels from artist=", std::quoted(query));
  const std::string url =
      std::string{kSearchUrl} + util::EncodeUrlQuery(query) + std::string{kChannelsFilter};

  return ListEntries(
      url, kSearchMaxChannels,
      [&channels](const nlohmann::json& info) { return ParseChannels(info, channels); }, cancel);
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ExtractReleases(const std::string& channel_id,
                                          std::vector<model::YoutubeRelease>& releases,
                                          const std::atomic<bool>* cancel) {
  INFO("Extract releases from channel=", channel_id);
  const std::string url = "https://www.youtube.com/channel/" + channel_id + "/releases";

  error::Code result = ListEntries(
      url, 0, [&releases](const nlohmann::json& info) { return ParseReleases(info, releases); },
      cancel);

  // Channel without any release does not even have the tab listing them
  if (result == error::kStreamUnavailable) {
    releases.clear();
    return error::kSuccess;
  }

  return result;
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::SearchPlaylists(const std::string& query,
                                          std::vector<model::YoutubeRelease>& playlists,
                                          const std::atomic<bool>* cancel) {
  INFO("Search playlists from artist=", std::quoted(query));

  // Most playlists found with only the artist name are compilations, instead of albums
  const std::string url = std::string{kSearchUrl} + util::EncodeUrlQuery(query + " album") +
                          std::string{kPlaylistsFilter};

  return ListEntries(
      url, kSearchMaxPlaylists,
      [&playlists](const nlohmann::json& info) { return ParseReleases(info, playlists); }, cancel);
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::MatchSongs(std::vector<model::Song>& songs, std::atomic<int>* progress,
                                     const std::atomic<bool>* cancel) {
  // Songs found before are not searched again
  std::vector<size_t> pending;
  for (size_t i = 0; i < songs.size(); ++i) {
    if (!songs[i].stream_info.has_value()) pending.push_back(i);
  }

  INFO("Match songs on YouTube, pending=", pending.size(), " total=", songs.size());
  if (pending.empty()) return error::kSuccess;

  auto program = util::FindExecutable(std::string{kProgram});

  if (!program) {
    WARN("Cannot find ", kProgram, " in PATH to match songs");
    return error::kStreamFetcherNotFound;
  }

  // Programs stop as soon as one of them is refused (or user cancels it)
  std::atomic<bool> stop = false;
  std::atomic<error::Code> failure = error::kSuccess;

  auto set_url = [&songs](size_t index, const std::string& id) {
    songs[index].stream_info =
        model::StreamInfo{.base_url = "https://www.youtube.com/watch?v=" + id};
  };

  // Each worker searches its own share of songs (and only writes to them)
  auto worker = [&](const std::vector<size_t>& share) {
    auto search = [&](const std::vector<size_t>& indexes, auto make_url) {
      std::vector<std::string> urls;
      for (size_t index : indexes) urls.push_back(make_url(songs[index]));

      std::map<std::string, nlohmann::json> results;
      error::Code result = Search(program->string(), urls, results, &stop);

      // Refused requests would only be refused again, so everybody stops
      if (result == error::kStreamBlocked || result == error::kStreamBlockedWithCookies ||
          result == error::kStreamCookiesFailed) {
        failure = result;
        stop = true;
      }

      return results;
    };

    // First, on YouTube Music (audio only, without any intro from music video)
    auto music_url = [](const model::Song& song) {
      return std::string{kMusicSearchUrl} + util::EncodeUrlQuery(song.artist + " " + song.title) +
             std::string{kMusicSongsSection};
    };

    auto results = search(share, music_url);
    std::vector<size_t> missing;

    for (size_t index : share) {
      auto it = results.find(music_url(songs[index]));
      auto id = it != results.end() ? PickByTitle(it->second, songs[index].title) : std::nullopt;

      if (!id) {
        missing.push_back(index);
        continue;
      }

      set_url(index, *id);
      if (progress != nullptr) ++*progress;
    }

    if (missing.empty() || stop) return;

    // Then, on YouTube itself
    auto video_url = [](const model::Song& song) {
      return std::string{kVideoSearchPrefix} + song.artist + " " + song.title;
    };

    results = search(missing, video_url);

    for (size_t index : missing) {
      auto it = results.find(video_url(songs[index]));
      if (auto id = it != results.end()
                        ? PickByDuration(it->second, songs[index].title, songs[index].duration)
                        : std::nullopt;
          id) {
        set_url(index, *id);
      }

      if (progress != nullptr) ++*progress;
    }
  };

  // Split songs between workers, so each one runs a single program for all of its songs
  const size_t num_workers = std::min(pending.size(), static_cast<size_t>(kMaxParallelSearches));
  std::vector<std::vector<size_t>> shares(num_workers);

  for (size_t i = 0; i < pending.size(); ++i) shares[i % num_workers].push_back(pending[i]);

  std::vector<std::thread> workers;
  for (const auto& share : shares) workers.emplace_back(worker, share);

  // Keep watching if user has canceled it
  std::thread watcher([&stop, cancel] {
    while (!stop) {
      if (cancel != nullptr && *cancel) stop = true;
      std::this_thread::sleep_for(kCancelCheckInterval);
    }
  });

  for (auto& thread : workers) thread.join();

  const bool canceled = cancel != nullptr && *cancel;
  stop = true;
  watcher.join();

  const auto found = std::count_if(songs.begin(), songs.end(),
                                   [](const model::Song& song) { return song.stream_info; });
  INFO("Matched songs on YouTube, found=", found, " total=", songs.size(), " canceled=", canceled,
       " failure=", failure.load());

  if (canceled) return error::kStreamFetchFailed;
  return failure;
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::Search(const std::string& program,
                                 const std::vector<std::string>& searches,
                                 std::map<std::string, nlohmann::json>& results,
                                 const std::atomic<bool>* cancel) {
  std::string output;
  std::vector<std::string> options{"--flat-playlist", "--dump-single-json", "--no-warnings",
                                   "--playlist-end", std::to_string(kSongMaxResults)};

  if (error::Code result = Run(program, options, searches, output, cancel);
      result != error::kSuccess) {
    return result;
  }

  // Each search is printed in its own line, which tells what was searched
  std::istringstream lines{output};

  for (std::string line; std::getline(lines, line);) {
    auto info = nlohmann::json::parse(line, nullptr, /*allow_exceptions=*/false);
    auto entries = info.is_object() ? info.find("entries") : info.end();

    if (entries == info.end() || !entries->is_array()) continue;

    results[GetOr<std::string>(info, "original_url", "")] = *entries;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

std::optional<std::string> YtDlpWrapper::PickByTitle(const nlohmann::json& entries,
                                                     const std::string& title) {
  if (!entries.is_array()) return std::nullopt;

  // Try exact title first, as text in brackets may tell apart another version of the same song
  for (bool relaxed : {false, true}) {
    for (const auto& entry : entries) {
      const std::string id = GetOr<std::string>(entry, "id", "");
      const std::string found = GetOr<std::string>(entry, "title", "");

      if (!id.empty() && util::same_title(found, title, relaxed)) return id;
    }
  }

  return std::nullopt;
}

/* ********************************************************************************************** */

std::optional<std::string> YtDlpWrapper::PickByDuration(const nlohmann::json& entries,
                                                        const std::string& title,
                                                        uint32_t duration) {
  if (!entries.is_array() || duration == 0) return std::nullopt;

  auto is_live = [](const std::string& text) {
    static const std::regex kLive(R"(\blive\b)", std::regex::icase);
    return std::regex_search(text, kLive);
  };

  const bool live = is_live(title);

  std::optional<std::string> picked;
  uint32_t smallest = kMaxDurationDifference + 1;

  for (const auto& entry : entries) {
    const std::string id = GetOr<std::string>(entry, "id", "");
    const auto found = static_cast<uint32_t>(GetOr<double>(entry, "duration", 0));

    if (id.empty() || found == 0) continue;
    if (!live && is_live(GetOr<std::string>(entry, "title", ""))) continue;

    const uint32_t difference = found > duration ? found - duration : duration - found;

    if (difference < smallest) {
      smallest = difference;
      picked = id;
    }
  }

  return picked;
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ListEntries(
    const std::string& url, int max_entries,
    const std::function<error::Code(const nlohmann::json&)>& parse,
    const std::atomic<bool>* cancel) {
  auto program = util::FindExecutable(std::string{kProgram});

  if (!program) {
    WARN("Cannot find ", kProgram, " in PATH to list entries from URL=", url);
    return error::kStreamFetcherNotFound;
  }

  // Only list entries (much faster than extracting information from every one of them)
  std::string output;
  std::vector<std::string> options{"--flat-playlist", "--dump-single-json", "--no-warnings"};

  if (max_entries > 0) {
    options.insert(options.end(), {"--playlist-end", std::to_string(max_entries)});
  }

  if (error::Code result = Run(program->string(), options, {url}, output, cancel);
      result != error::kSuccess) {
    return result;
  }

  return parse(nlohmann::json::parse(output, nullptr, /*allow_exceptions=*/false));
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

    // Duration may be a floating point number (in seconds)
    song.duration = static_cast<uint32_t>(GetOr<double>(entry, "duration", 0));

    parsed.push_back(std::move(song));
  }

  INFO("Parsed playlist=", std::quoted(GetOr<std::string>(info, "title", "")),
       " with songs=", parsed.size(), " skipped=", skipped);

  songs = std::move(parsed);
  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ParseChannels(const nlohmann::json& info,
                                        std::vector<model::YoutubeChannel>& channels) {
  static const std::regex kTopic(R"(\s+-\s+Topic$)");

  auto entries = info.is_object() ? info.find("entries") : info.end();

  if (entries == info.end() || !entries->is_array()) {
    ERROR("Could not parse channels extracted from search");
    return error::kStreamFetchFailed;
  }

  std::vector<model::YoutubeChannel> parsed;

  for (const auto& entry : *entries) {
    if (!entry.is_object()) continue;

    const std::string id = GetOr<std::string>(entry, "channel_id", "");
    const std::string name = GetOr<std::string>(entry, "title", "");

    // Topic channel is created by YouTube for each artist, but it does not list any release
    if (id.empty() || name.empty() || std::regex_search(name, kTopic)) continue;

    auto followers = GetOr<double>(entry, "channel_follower_count", -1);

    parsed.push_back(model::YoutubeChannel{
        .id = id,
        .name = name,
        .followers =
            followers >= 0 ? std::optional{static_cast<uint64_t>(followers)} : std::nullopt,
        .verified = GetOr<bool>(entry, "channel_is_verified", false),
    });
  }

  // Official channel from artist is verified, so it is the one that user is looking for
  std::stable_partition(parsed.begin(), parsed.end(),
                        [](const model::YoutubeChannel& channel) { return channel.verified; });

  INFO("Parsed channels from search, found=", parsed.size());

  channels = std::move(parsed);
  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code YtDlpWrapper::ParseReleases(const nlohmann::json& info,
                                        std::vector<model::YoutubeRelease>& releases) {
  auto entries = info.is_object() ? info.find("entries") : info.end();

  if (entries == info.end() || !entries->is_array()) {
    ERROR("Could not parse releases extracted from URL");
    return error::kStreamFetchFailed;
  }

  std::vector<model::YoutubeRelease> parsed;

  for (const auto& entry : *entries) {
    if (!entry.is_object()) continue;

    const std::string id = GetOr<std::string>(entry, "id", "");
    const std::string title = GetOr<std::string>(entry, "title", "");
    const std::string url = GetOr<std::string>(entry, "url", "");

    // Search may also find channels, which are not a playlist
    if (id.empty() || title.empty() || url.find("list=") == std::string::npos) continue;

    parsed.push_back(model::YoutubeRelease{
        .id = id,
        .title = title,
        .uploader = GetOr<std::string>(entry, "uploader", GetOr<std::string>(entry, "channel", "")),
    });
  }

  INFO("Parsed releases from URL, found=", parsed.size());

  releases = std::move(parsed);
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
                              const std::vector<std::string>& urls, std::string& output,
                              const std::atomic<bool>* cancel) {
  // Only used for logging
  std::string url;
  for (const auto& entry : urls) url += (url.empty() ? "" : " ") + entry;

  auto execute = [&](bool with_cookies) {
    std::vector<std::string> args{program};
    args.insert(args.end(), options.begin(), options.end());

    // Browser is a single argument, so program is the only one to make sense of its content
    if (with_cookies) args.insert(args.end(), {std::string{kOptionCookies}, cookies_browser_});

    args.emplace_back("--");
    args.insert(args.end(), urls.begin(), urls.end());

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

  //! Texts printed by program when video cannot be watched by anyone (or only by its owner), or
  //! when channel does not have the tab asked for (e.g. "does not have a releases tab")
  static constexpr std::array kUnavailable{"Video unavailable"sv, "Private video"sv,
                                           "has been removed"sv,  "is not available"sv,
                                           "does not exist"sv,    "does not have a"sv};

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

  if (error::Code result =
          Run(program->string(), {"--dump-single-json", "--no-playlist", "--no-warnings"}, {url},
              output);
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

  if (!song.artist.empty()) {
    // Song name is usually the one in quotes, e.g. "Artist - "Song"" or ""Song" - Artist"
    bool artist_quoted = Unquote(song.artist);
    Unquote(song.title);

    // Some videos have song name before artist, which is noticed when only the latter is the
    // uploader
    auto is_uploader = [&metadata](const std::string& name) {
      if (!metadata.is_object()) return false;

      return std::any_of(std::begin(kUploaderKeys), std::end(kUploaderKeys), [&](const char* key) {
        return EqualsIgnoreCase(CleanUploader(GetOr<std::string>(metadata, key, "")), name);
      });
    };

    if (artist_quoted || (is_uploader(song.title) && !is_uploader(song.artist))) {
      std::swap(song.artist, song.title);
    }

    return;
  }

  if (!metadata.is_object()) return;

  // Video title has no artist, so use the first valid one from metadata
  for (const char* key : {"artist", "uploader", "channel"}) {
    if (std::string artist =
            CleanUploader(util::trim(util::filter_emoji(GetOr<std::string>(metadata, key, ""))));
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
  info.available_at = std::chrono::seconds{GetOr<int64_t>(entry, "available_at", 0)};
  info.expires_at = GetExpiration(info.streaming_url).value_or(std::chrono::seconds{0});

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
