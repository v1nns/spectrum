#include "audio/lyric/lyric_finder.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "model/application_error.h"

#ifndef SPECTRUM_DEBUG
#include "web/driver/curl_wrapper.h"
#include "web/driver/libxml_wrapper.h"
#else
#include "debug/dummy_fetcher.h"
#include "debug/dummy_parser.h"
#endif

#include "util/formatter.h"
#include "util/logger.h"

namespace lyric {

namespace {

//! Difference in seconds between two durations
uint32_t Difference(uint32_t a, uint32_t b) { return a > b ? a - b : b - a; }

}  // namespace

std::unique_ptr<LyricFinder> LyricFinder::Create(web::UrlFetcher* fetcher,
                                                 web::HtmlParser* parser) {
  LOG("Create new instance of lyric finder");

#ifndef SPECTRUM_DEBUG
  // Create fetcher object
  auto ft = fetcher != nullptr ? std::unique_ptr<web::UrlFetcher>(std::move(fetcher))
                               : std::make_unique<driver::CURLWrapper>();

  // Create parser object
  auto ps = parser != nullptr ? std::unique_ptr<web::HtmlParser>(std::move(parser))
                              : std::make_unique<driver::LIBXMLWrapper>();
#else
  // Create fetcher object
  auto ft = std::make_unique<driver::DummyFetcher>();

  // Create parser object
  auto ps = std::make_unique<driver::DummyParser>();
#endif

  // Simply extend the LyricFinder class, as we do not want to expose the default constructor,
  // neither do we want to use std::make_unique explicitly calling operator new()
  struct MakeUniqueEnabler : public LyricFinder {
    explicit MakeUniqueEnabler(std::unique_ptr<web::UrlFetcher>&& fetcher,
                               std::unique_ptr<web::HtmlParser>&& parser)
        : LyricFinder(std::move(fetcher), std::move(parser)) {}
  };

  // Instantiate LyricFinder
  return std::make_unique<MakeUniqueEnabler>(std::move(ft), std::move(ps));
}

/* ********************************************************************************************** */

LyricFinder::LyricFinder(std::unique_ptr<web::UrlFetcher>&& fetcher,
                         std::unique_ptr<web::HtmlParser>&& parser)
    : fetcher_{std::move(fetcher)}, parser_{std::move(parser)} {}

/* ********************************************************************************************** */

void LyricFinder::SetCancelCheck(const web::UrlFetcher::CancelCheck& check) {
  cancel_check_ = check;

  // Fetcher may not exist when this class is mocked
  if (fetcher_) fetcher_->SetCancelCheck(check);
}

/* ********************************************************************************************** */

std::string LyricFinder::CleanTitle(const std::string& artist, const std::string& title) {
  // Words that are related to video, and not to song name
  static const std::regex kVideoWords(
      R"(\b(official|lyrics?|video|audio|visuali[sz]er|hd|4k|remaster(ed)?)\b)", std::regex::icase);

  // Parentheses with featured artists, video-related words or another version of the song, e.g.
  // "(feat. X)", "(Official Video)", "(X Remix)" or "(Ao Vivo)" (innermost ones, as they may be
  // nested)
  static const std::regex kParentheses(
      R"(\s*\([^\(\)]*\b(feat|ft|featuring|with|official|lyrics?|video|audio|visuali[sz]er|hd|)"
      R"(4k|remaster(ed)?|mix(ed)?|remix|edit|extended|slowed|sped|reverb|nightcore|instrumental|)"
      R"(unreleased|version|prod|live|ao vivo|en vivo)\b[^\(\)]*\))",
      std::regex::icase);

  // Square brackets are not used in song names, e.g. "[Official Video]" or "[Deep House | Chill]"
  static const std::regex kSquareBrackets(R"(\s*\[[^\[\]]*\])");

  // Hashtags, e.g. "Song #Lyrics"
  static const std::regex kHashtags(R"(\s*#\S+)");

  // Separators between sections, e.g. "Song | Genre" or "Artist - Song" (hyphen, en dash, em dash
  // and bullet, surrounded by spaces so hyphens from names like "Jay-Z" are not taken as one)
  static const std::regex kSeparators(R"(\s*\|\s*|\s+(-|–|—|•)\s+)");

  // Featured artists without brackets, e.g. "Song feat. X"
  static const std::regex kFeaturing(R"(\s+(feat\.?|ft\.|featuring)\s.*$)", std::regex::icase);

  auto lowercase = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
  };

  const std::string lower_artist = lowercase(artist);
  std::string first_section;
  std::string cleaned;

  // Remove brackets before splitting into sections, as they may contain a separator
  std::string text = std::regex_replace(title, kSquareBrackets, "");
  for (std::string previous; previous != text;) {
    previous = text;
    text = std::regex_replace(text, kParentheses, "");
  }
  text = std::regex_replace(text, kHashtags, "");

  // Split into sections and keep the first one that is about the song itself
  for (std::sregex_token_iterator it(text.begin(), text.end(), kSeparators, -1), end; it != end;
       ++it) {
    std::string section = util::trim(std::regex_replace(it->str(), kFeaturing, ""));

    if (section.empty()) continue;
    if (first_section.empty()) first_section = section;

    bool has_artist =
        !lower_artist.empty() && lowercase(section).find(lower_artist) != std::string::npos;

    if (!has_artist && !std::regex_search(section, kVideoWords)) {
      cleaned = section;
      break;
    }
  }

  // Every section was discarded, so use at least the first one
  return cleaned.empty() ? first_section : cleaned;
}

/* ********************************************************************************************** */

std::string LyricFinder::FirstArtist(const std::string& artist) {
  // Everything from the first separator between artists, e.g. "A, B", "A & B", "A + B", "A x B" or
  // "A feat. B"
  static const std::regex kOtherArtists(
      R"(\s*(,|&|\+|\s(x|feat\.?|ft\.|featuring|with|vs\.?)\s).*$)", std::regex::icase);

  return util::trim(std::regex_replace(artist, kOtherArtists, ""));
}

/* ********************************************************************************************** */

SearchResult LyricFinder::Search(const std::string& artist, const std::string& raw_title,
                                 uint32_t duration) {
  const std::string title = CleanTitle(artist, raw_title);
  LOG("Started fetching song by artist=", artist, " title=", title, " (from ", raw_title, ")");

  SearchResult result = SearchEngines(artist, title);
  std::string found_artist = artist;

  // Song credited to more than one artist is usually known by search engines only by the first one.
  // But a shorter name may match another song, so duration must match too
  if (result.status == SearchResult::Status::NotFound && duration > 0) {
    if (std::string first = FirstArtist(artist); !first.empty() && first != artist) {
      LOG("Retry search using only the first artist=", first, " and duration=", duration);
      if (SearchResult retry = SearchEngines(first, title, duration);
          retry.status == SearchResult::Status::Found) {
        result = std::move(retry);
        found_artist = first;
      }
    }
  }

  // Lyrics found may belong to another recording of the song (e.g. studio version for a live one),
  // so look for synced lyrics from a version matching song duration. Otherwise, show only plain
  // lyrics, as synced ones would not follow song (unless there are no plain lyrics)
  if (duration > 0 && result.duration > 0 &&
      Difference(duration, result.duration) > kMaxSyncedDurationDifference) {
    if (model::SyncedLyric synced = SearchSyncedVersion(found_artist, title, duration);
        !synced.empty()) {
      result.synced = std::move(synced);
    } else if (!result.lyrics.empty() && !result.synced.empty()) {
      LOG("Discard synced lyrics, as they belong to a song with duration=", result.duration);
      result.synced.clear();
    }
  }

  return result;
}

/* ********************************************************************************************** */

SearchResult LyricFinder::SearchEngines(const std::string& artist, const std::string& title,
                                        uint32_t duration) {
  bool fetched_any = false;
  bool failed_any = false;

  for (const auto& engine : engines_) {
    // Search engine would not check duration, so it could find lyrics from another song
    if (duration > 0 && !engine->ChecksDuration()) continue;

    // Result is discarded by owner when search is canceled, so just stop it
    if (cancel_check_ && cancel_check_()) {
      LOG("Canceled search for song lyrics");
      return SearchResult{.status = SearchResult::Status::FetchFailed};
    }

    // Fetcher appends to buffer, so each search engine must have its own
    std::string buffer;

    // Fetch content from search engine
    auto result = FetchWithRetry(engine->FormatSearchUrl(artist, title, duration), buffer);

    // Search engine was reached, it just does not have this song
    if (result == error::kUrlNotFound) {
      LOG("Song not found using search engine=", *engine);
      fetched_any = true;
      continue;
    }

    if (result != error::kSuccess) {
      WARN("Failed to fetch URL content, error code=", result);
      failed_any = true;
      continue;
    }

    fetched_any = true;

    // Extract content to search for lyric
    model::SongLyric formatted;
    if (model::SongLyric raw = engine->ExtractLyrics(buffer, *parser_); !raw.empty()) {
      formatted = engine->FormatLyrics(raw);
    }

    model::SyncedLyric synced = engine->ExtractSyncedLyrics(buffer);

    if (!formatted.empty() || !synced.empty()) {
      INFO("Found lyrics using search engine=", *engine, " synced=", !synced.empty());
      return SearchResult{.status = SearchResult::Status::Found,
                          .lyrics = std::move(formatted),
                          .synced = std::move(synced),
                          .duration = engine->ExtractDuration(buffer)};
    }
  }

  // Lyrics are surely not found only when every engine was reached (otherwise, user may retry)
  return SearchResult{.status = fetched_any && !failed_any ? SearchResult::Status::NotFound
                                                           : SearchResult::Status::FetchFailed};
}

/* ********************************************************************************************** */

model::SyncedLyric LyricFinder::SearchSyncedVersion(const std::string& artist,
                                                    const std::string& title, uint32_t duration) {
  for (const auto& engine : engines_) {
    std::string url = engine->FormatVersionsUrl(artist, title);
    if (url.empty()) continue;

    // Result is discarded by owner when search is canceled, so just stop it
    if (cancel_check_ && cancel_check_()) return model::SyncedLyric{};

    std::string buffer;
    if (auto result = FetchWithRetry(url, buffer); result != error::kSuccess) {
      WARN("Failed to fetch versions of song, error code=", result);
      continue;
    }

    std::string version = engine->ExtractClosestVersion(buffer, duration);
    if (version.empty()) continue;

    const uint32_t found = engine->ExtractDuration(version);
    if (Difference(duration, found) <= kMaxSyncedDurationDifference) {
      INFO("Found synced lyrics from another version of song using search engine=", *engine,
           " duration=", found);
      return engine->ExtractSyncedLyrics(version);
    }

    LOG("No version of song with synced lyrics matching duration, closest one has duration=",
        found);
  }

  return model::SyncedLyric{};
}

/* ********************************************************************************************** */

error::Code LyricFinder::FetchWithRetry(const std::string& url, std::string& output) {
  std::chrono::milliseconds delay = retry_delay_;

  for (int retry = 0;; retry++) {
    // Fetcher appends to output, so discard content from previous attempt
    output.clear();

    auto result = fetcher_->Fetch(url, output);
    if (result != error::kUrlUnavailable || retry == kMaxRetries) return result;

    LOG("Server is unavailable, retry fetching URL in ", delay.count(), "ms");
    if (!WaitBeforeRetry(delay)) return result;

    delay *= 2;
  }
}

/* ********************************************************************************************** */

bool LyricFinder::WaitBeforeRetry(std::chrono::milliseconds delay) const {
  const auto deadline = std::chrono::steady_clock::now() + delay;

  while (std::chrono::steady_clock::now() < deadline) {
    if (cancel_check_ && cancel_check_()) {
      LOG("Canceled search for song lyrics while waiting to retry");
      return false;
    }

    std::this_thread::sleep_for(
        std::min(kCancelCheckInterval, std::chrono::duration_cast<std::chrono::milliseconds>(
                                           deadline - std::chrono::steady_clock::now())));
  }

  return true;
}

}  // namespace lyric
