/**
 * \file
 * \brief  Class for Search Engine config
 */

#ifndef INCLUDE_AUDIO_LYRIC_SEARCH_CONFIG_H_
#define INCLUDE_AUDIO_LYRIC_SEARCH_CONFIG_H_

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "model/song.h"
#include "web/base/html_parser.h"

namespace lyric {

//! Must match the list size of available search engines
static constexpr int kMaxEngines = 2;

//! Forward declaration
class SearchConfig;
using Engine = std::unique_ptr<SearchConfig>;
using Config = std::array<Engine, kMaxEngines>;

/**
 * @brief Interface for search engine configuration
 */
class SearchConfig {
 public:
  virtual ~SearchConfig() = default;

  //! Overloaded operator
  friend std::ostream &operator<<(std::ostream &out, const SearchConfig &s) {
    return out << s.name();
  }

  // -------------------------  These must be implemented by derived class -------------------------

  //! Return search engine name
  virtual std::string_view name() const = 0;

  //! Return URL for search, it is used to fetch song lyric
  virtual std::string url() const = 0;

  //! Return XPath to web scrap content from HTTP GET
  virtual std::string xpath() const = 0;

  /**
   * @brief Format search URL with artist name and song title
   * @param artist Artist name
   * @param title Song title
   */
  virtual std::string FormatSearchUrl(const std::string &artist,
                                      const std::string &title) const = 0;

  /**
   * @brief Filter webscraping content to an expected song lyrics format
   * @param raw HTML content
   * @return Song lyrics filtered
   */
  virtual model::SongLyric FormatLyrics(const model::SongLyric &raw) const = 0;

  // -----------------------------  This may be overridden by derived class ------------------------

  /**
   * @brief Extract raw song lyrics from content fetched from search URL (by default, web scrap it
   * as HTML using XPath)
   * @param content Content fetched from search URL
   * @param parser HTML parser
   * @return Raw song lyrics (empty if not found)
   */
  virtual model::SongLyric ExtractLyrics(const std::string &content,
                                         web::HtmlParser &parser) const {
    return parser.Parse(content, xpath());
  }

  /**
   * @brief Create a configuration containing all available search engines to use it
   * @return An array of search engines
   */
  static Config Create();
};

/* ********************************************************************************************** */

/**
 * @brief Search configurations to get lyrics from LRCLIB (JSON API, so there is no web scraping)
 */
class LRCLIB : public SearchConfig {
  static constexpr std::string_view kEngineName = "LRCLIB";  //!< Search engine name

 public:
  //! Return search engine name
  std::string_view name() const override { return kEngineName; }

  //! Return URL for search, it is used to fetch song lyric
  std::string url() const override { return url_; }

  //! Return XPath to web scrap content from HTTP GET (not used, as content is not HTML)
  std::string xpath() const override { return std::string{}; }

  /**
   * @brief Format search URL with artist name and song title
   * @param artist Artist name
   * @param title Song title
   */
  std::string FormatSearchUrl(const std::string &artist, const std::string &name) const override;

  /**
   * @brief Split raw song lyrics into paragraphs
   * @param raw Raw song lyrics
   * @return Song lyrics filtered
   */
  model::SongLyric FormatLyrics(const model::SongLyric &raw) const override;

  /**
   * @brief Extract raw song lyrics from JSON content
   * @param content Content fetched from search URL
   * @param parser HTML parser (not used)
   * @return Raw song lyrics (empty if not found)
   */
  model::SongLyric ExtractLyrics(const std::string &content,
                                 web::HtmlParser &parser) const override;

 private:
  const std::string url_ = "https://lrclib.net/api/get";
};

/* ********************************************************************************************** */

/**
 * @brief Search configurations to web scrap from AZLyrics
 */
class AZLyrics : public SearchConfig {
  static constexpr std::string_view kEngineName = "AZLyrics";  //!< Search engine name

 public:
  //! Return search engine name
  std::string_view name() const override { return kEngineName; }

  //! Return URL for search, it is used to fetch song lyric
  std::string url() const override { return url_; }

  //! Return XPath to web scrap content from HTTP GET
  std::string xpath() const override { return xpath_; }

  /**
   * @brief Format search URL with artist name and song title (website uses only lowercase letters
   * and digits, without accents and without the article in the beginning of artist name)
   * @param artist Artist name
   * @param title Song title
   */
  std::string FormatSearchUrl(const std::string &artist, const std::string &name) const override;

  /**
   * @brief Filter webscraping content to an expected song lyrics format
   * @param raw HTML content
   * @return Song lyrics filtered
   */
  model::SongLyric FormatLyrics(const model::SongLyric &raw) const override;

 private:
  // Web scrap lyrics from azlyrics based on these DOM component:
  //   the next div after matched div[class="ringtone"]
  const std::string url_ = "https://www.azlyrics.com/lyrics/";
  const std::string xpath_ = "//div[@class=\"ringtone\"]/following::div[1]";
};

}  // namespace lyric
#endif  // INCLUDE_AUDIO_LYRIC_SEARCH_CONFIG_H_
