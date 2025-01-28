/**
 * \file
 * \brief Interface class for HTML parsing support
 */

#ifndef INCLUDE_WEB_BASE_HTML_PARSER_H_
#define INCLUDE_WEB_BASE_HTML_PARSER_H_

#include <string>

#include "model/song.h"

namespace web {

/**
 * @brief Common interface to parse HTML content into a DOM tree
 */
class HtmlParser {
 public:
  /**
   * @brief Construct a new HtmlParser object
   */
  HtmlParser() = default;

  /**
   * @brief Destroy the HtmlParser object
   */
  virtual ~HtmlParser() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Parse buffer data based on the given XPath
   * @param data Buffer data
   * @param xpath XPath to find
   * @return Song lyrics parsed from buffer data
   */
  virtual model::SongLyric Parse(const std::string &data, const std::string &xpath) = 0;
};

}  // namespace web
#endif  // INCLUDE_WEB_BASE_HTML_PARSER_H_
