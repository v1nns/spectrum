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

#include "audio/lyric/base/html_parser.h"

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

  friend std::ostream &operator<<(std::ostream &out, const SearchConfig &s) {
    return out << s.name();
  }

  //! These must be implemented by derived class
  // TODO: doc these methods

  virtual std::string_view name() const = 0;
  virtual std::string url() const = 0;
  virtual std::string xpath() const = 0;

  virtual std::string FormatSearchUrl(const std::string &artist, const std::string &name) const = 0;
  virtual SongLyric FormatLyrics(const SongLyric &raw) const = 0;

  /**
   * @brief Create a configuration containing all available search engines to use it
   * @return An array of search engines
   */
  static Config Create();
};

/* ********************************************************************************************** */

/**
 * @brief Search configurations to web scrap from Google
 */
class Google : public SearchConfig {
  static constexpr std::string_view kEngineName = "Google";

 public:
  std::string_view name() const { return kEngineName; }

  std::string url() const override { return url_; }

  std::string xpath() const override { return xpath_; }

  std::string FormatSearchUrl(const std::string &artist, const std::string &name) const override;
  SongLyric FormatLyrics(const SongLyric &raw) const override;

 private:
  // Web scrap lyrics from google based on these DOM components:
  //   div[class="BNeawe iBp4i AP7Wnd"] (not used)
  //   div[class="BNeawe tAd8D AP7Wnd"] (used)
  const std::string url_ = "https://www.google.com/search?q=lyric+";
  const std::string xpath_ = "(//div[@class=\"BNeawe tAd8D AP7Wnd\"])[last()-1]";
};

/* ********************************************************************************************** */

/**
 * @brief Search configurations to web scrap from AZLyrics
 */
class AZLyrics : public SearchConfig {
  static constexpr std::string_view kEngineName = "AZLyrics";

 public:
  std::string_view name() const { return kEngineName; }

  std::string url() const override { return url_; }

  std::string xpath() const override { return xpath_; }

  std::string FormatSearchUrl(const std::string &artist, const std::string &name) const override;
  SongLyric FormatLyrics(const SongLyric &raw) const override;

 private:
  // Web scrap lyrics from azlyrics based on these DOM component:
  //   the next div after matched div[class="ringtone"]
  const std::string url_ = "https://www.azlyrics.com/lyrics/";
  const std::string xpath_ = "//div[@class=\"ringtone\"]/following::div[1]";
};

}  // namespace lyric
#endif  // INCLUDE_AUDIO_LYRIC_SEARCH_CONFIG_H_