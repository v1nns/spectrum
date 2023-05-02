/**
 * \file
 * \brief  Class to wrap libxml++ funcionalities
 */

#ifndef INCLUDE_AUDIO_LYRIC_LIBXML_WRAPPER_H_
#define INCLUDE_AUDIO_LYRIC_LIBXML_WRAPPER_H_

#include <libxml++/libxml++.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>

#include <memory>
#include <string>

#include "audio/lyric/base/html_parser.h"

namespace driver {

/**
 * @brief Class to manage libxml++ resources and perform content parsing
 */
class LIBXMLWrapper : public driver::HtmlParser {
 public:
  /**
   * @brief Parse buffer data based on the given XPath
   * @param data Buffer data
   * @param xpath XPath to find
   * @return Song lyrics parsed from buffer data
   */
  lyric::SongLyric Parse(const std::string &data, const std::string &xpath) override;

 private:
  /**
   * @brief Filter only text nodes to emplace back on lyrics
   * @param node XML node
   * @param lyric Song lyrics (out)
   */
  void ScrapContent(const xmlpp::Node *node, lyric::SongLyric &lyric);

  //! Smart pointer to manage libxml resources
  using XmlDocGuard = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>;

  //! Smart pointer to manage libxml++ resources
  using ElementGuard = std::unique_ptr<xmlpp::Element>;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_LYRIC_LIBXML_WRAPPER_H_