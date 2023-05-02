/**
 * \file
 * \brief  Class for Lyric Finder
 */

#ifndef INCLUDE_AUDIO_LYRIC_LYRIC_FINDER_H_
#define INCLUDE_AUDIO_LYRIC_LYRIC_FINDER_H_

#include <memory>
#include <string>

#include "audio/lyric/base/html_parser.h"
#include "audio/lyric/base/url_fetcher.h"
#include "audio/lyric/search_config.h"

#ifdef ENABLE_TESTS
namespace {
class LyricFinderTest;
}
#endif

namespace lyric {

/**
 * @brief Responsible to fetch content from search engines and web scrap song lyrics from it
 */
class LyricFinder {
  /**
   * @brief Construct a new LyricFinder object
   * @param fetcher Pointer to URL fetcher interface
   * @param parser Pointer to HTML parser interface
   */
  explicit LyricFinder(std::unique_ptr<driver::UrlFetcher>&& fetcher,
                       std::unique_ptr<driver::HtmlParser>&& parser);

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return LyricFinder object
   * @param fetcher Pass fetcher to be used within LyricFinder (optional)
   * @param parser Pass parser to be used within LyricFinder (optional)
   * @return std::unique_ptr<LyricFinder> LyricFinder instance
   */
  static std::unique_ptr<LyricFinder> Create(driver::UrlFetcher* fetcher = nullptr,
                                             driver::HtmlParser* parser = nullptr);

  /**
   * @brief Destroy the LyricFinder object
   */
  virtual ~LyricFinder() = default;

  //! Remove these
  LyricFinder(const LyricFinder& other) = delete;             // copy constructor
  LyricFinder(LyricFinder&& other) = delete;                  // move constructor
  LyricFinder& operator=(const LyricFinder& other) = delete;  // copy assignment
  LyricFinder& operator=(LyricFinder&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Search for lyrics by fetching the search engine and web scraping it
   * @param artist Artist name
   * @param title Song name
   * @return Song lyrics
   */
  SongLyric Search(const std::string& artist, const std::string& title);

  /* ******************************************************************************************** */
  //! Variables
 private:
  Config engines_ = SearchConfig::Create();      //!< Search engine settings
  std::unique_ptr<driver::UrlFetcher> fetcher_;  //!< URL fetcher
  std::unique_ptr<driver::HtmlParser> parser_;   //!< HTML parser

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::LyricFinderTest;
#endif
};

}  // namespace lyric
#endif  // INCLUDE_AUDIO_LYRIC_LYRIC_FINDER_H_
