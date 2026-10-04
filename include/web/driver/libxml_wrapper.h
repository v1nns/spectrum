/**
 * \file
 * \brief  Class to wrap libxml++ funcionalities
 */

#ifndef INCLUDE_WEB_DRIVER_LIBXML_WRAPPER_H_
#define INCLUDE_WEB_DRIVER_LIBXML_WRAPPER_H_

#include <libxml++/libxml++.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>

#include <memory>
#include <string>

#include "model/song.h"
#include "web/base/html_parser.h"

namespace driver {

/**
 * @brief Class to manage libxml++ resources and perform content parsing
 */
class LIBXMLWrapper : public web::HtmlParser {
 public:
  /**
   * @brief Parse buffer data based on the given XPath
   * @param data Buffer data
   * @param xpath XPath to find
   * @return Song lyrics parsed from buffer data
   */
  model::SongLyric Parse(const std::string &data, const std::string &xpath) override;

 private:
  /**
   * @brief Filter only text nodes to emplace back on lyrics
   * @param node XML node
   * @param lyric Song lyrics (out)
   */
  void ScrapContent(const xmlpp::Node *node, model::SongLyric &lyric);

  //! Smart pointer to manage libxml resources
  using XmlDocGuard = std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>;
};

}  // namespace driver
#endif  // INCLUDE_WEB_DRIVER_LIBXML_WRAPPER_H_
