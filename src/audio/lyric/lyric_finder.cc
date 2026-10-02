#include "audio/lyric/lyric_finder.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
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

  // Brackets with featured artists or video-related words, e.g. "(feat. X)" or "[Official Video]"
  static const std::regex kBrackets(
      R"(\s*[\(\[]\s*((feat|ft)\b\.?|)"
      R"((featuring|official|lyrics?|video|audio|visuali[sz]er|hd|4k|remaster(ed)?)\b))"
      R"([^\)\]]*[\)\]])",
      std::regex::icase);

  // Featured artists without brackets, e.g. "Song feat. X"
  static const std::regex kFeaturing(R"(\s+(feat\.?|ft\.|featuring)\s.*$)", std::regex::icase);

  auto lowercase = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
  };

  const std::string lower_artist = lowercase(artist);
  std::string first_section;
  std::string cleaned;

  // Split into sections by "|" and keep the first one that is about the song itself
  std::string::size_type start = 0;
  while (start <= title.size()) {
    std::string::size_type end = title.find('|', start);
    if (end == std::string::npos) end = title.size();

    std::string section = std::regex_replace(title.substr(start, end - start), kBrackets, "");
    section = util::trim(std::regex_replace(section, kFeaturing, ""));
    start = end + 1;

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

SearchResult LyricFinder::Search(const std::string& artist, const std::string& raw_title) {
  const std::string title = CleanTitle(artist, raw_title);
  LOG("Started fetching song by artist=", artist, " title=", title, " (from ", raw_title, ")");
  std::string buffer;
  bool fetched_any = false;

  for (const auto& engine : engines_) {
    // Result is discarded by owner when search is canceled, so just stop it
    if (cancel_check_ && cancel_check_()) {
      LOG("Canceled search for song lyrics");
      return SearchResult{.status = SearchResult::Status::FetchFailed};
    }

    // Fetch content from search engine
    if (auto result = fetcher_->Fetch(engine->FormatSearchUrl(artist, title), buffer);
        result != error::kSuccess) {
      ERROR("Failed to fetch URL content, error code=", result);
      continue;
    }

    fetched_any = true;

    // Web scrap content to search for lyric
    if (model::SongLyric raw = parser_->Parse(buffer, engine->xpath()); !raw.empty()) {
      if (model::SongLyric formatted = engine->FormatLyrics(raw); !formatted.empty()) {
        LOG("Found lyrics using search engine=", *engine);
        return SearchResult{.status = SearchResult::Status::Found, .lyrics = std::move(formatted)};
      }
    }
  }

  // Distinguish between not being able to reach any engine and not finding lyrics on them
  return SearchResult{.status = fetched_any ? SearchResult::Status::NotFound
                                            : SearchResult::Status::FetchFailed};
}

}  // namespace lyric
