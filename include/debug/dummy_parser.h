/**
 * \file
 * \brief Dummy class for HTML parsing support
 */

#ifndef INCLUDE_DEBUG_DUMMY_PARSER_H_
#define INCLUDE_DEBUG_DUMMY_PARSER_H_

#include <string>

#include "model/application_error.h"
#include "model/song.h"

namespace driver {

/**
 * @brief Dummy implementation
 */
class DummyParser : public web::HtmlParser {
 public:
  /**
   * @brief Construct a new DummyParser object
   */
  DummyParser() = default;

  /**
   * @brief Destroy the DummyParser object
   */
  virtual ~DummyParser() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Parse buffer data based on the given XPath
   * @param data Buffer data
   * @param xpath XPath to find
   * @return Song lyrics parsed from buffer data
   */
  model::SongLyric Parse(const std::string &data, const std::string &xpath) override {
    return model::SongLyric{};
  }
};

}  // namespace driver
#endif  // INCLUDE_DEBUG_DUMMY_PARSER_H_
