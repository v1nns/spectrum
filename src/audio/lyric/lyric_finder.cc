#include "audio/lyric/lyric_finder.h"

#include "model/application_error.h"

#ifndef SPECTRUM_DEBUG
#include "audio/lyric/driver/curl_wrapper.h"
#include "audio/lyric/driver/libxml_wrapper.h"
#else
#include "debug/dummy_fetcher.h"
#include "debug/dummy_parser.h"
#endif

#include "util/logger.h"

namespace lyric {

std::unique_ptr<LyricFinder> LyricFinder::Create(driver::UrlFetcher* fetcher,
                                                 driver::HtmlParser* parser) {
  LOG("Create new instance of lyric finder");

#ifndef SPECTRUM_DEBUG
  // Create fetcher object
  auto ft = fetcher != nullptr ? std::unique_ptr<driver::UrlFetcher>(std::move(fetcher))
                               : std::make_unique<driver::CURLWrapper>();

  // Create parser object
  auto ps = parser != nullptr ? std::unique_ptr<driver::HtmlParser>(std::move(parser))
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
    explicit MakeUniqueEnabler(std::unique_ptr<driver::UrlFetcher>&& fetcher,
                               std::unique_ptr<driver::HtmlParser>&& parser)
        : LyricFinder(std::move(fetcher), std::move(parser)) {}
  };

  // Instantiate LyricFinder
  return std::make_unique<MakeUniqueEnabler>(std::move(ft), std::move(ps));
}

/* ********************************************************************************************** */

LyricFinder::LyricFinder(std::unique_ptr<driver::UrlFetcher>&& fetcher,
                         std::unique_ptr<driver::HtmlParser>&& parser)
    : fetcher_{std::move(fetcher)}, parser_{std::move(parser)} {}

/* ********************************************************************************************** */

SongLyric LyricFinder::Search(const std::string& artist, const std::string& title) {
  LOG("Started fetching song by artist=", artist, " title=", title);
  std::string buffer;
  SongLyric lyrics;

  for (const auto& engine : engines_) {
    // Fetch content from search engine
    if (auto result = fetcher_->Fetch(engine->FormatSearchUrl(artist, title), buffer);
        result != error::kSuccess) {
      ERROR("Failed to fetch URL content, error code=", result);
      continue;
    }

    // Web scrap content to search for lyric
    if (SongLyric raw = parser_->Parse(buffer, engine->xpath()); !raw.empty()) {
      if (SongLyric formatted = engine->FormatLyrics(raw); !formatted.empty()) {
        LOG("Found lyrics using search engine=", *engine);
        lyrics.swap(formatted);
        break;
      }
    }
  }

  return lyrics;
}

}  // namespace lyric
