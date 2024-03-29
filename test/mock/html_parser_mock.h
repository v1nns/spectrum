/**
 * \file
 * \brief  Mock class for HTML parsing API
 */

#ifndef INCLUDE_TEST_HTML_PARSER_MOCK_H_
#define INCLUDE_TEST_HTML_PARSER_MOCK_H_

#include <gmock/gmock-function-mocker.h>

#include "audio/lyric/base/html_parser.h"

namespace {

class HtmlParserMock final : public driver::HtmlParser {
 public:
  MOCK_METHOD(lyric::SongLyric, Parse, (const std::string&, const std::string&), (override));
};

}  // namespace
#endif  // INCLUDE_TEST_HTML_PARSER_MOCK_H_