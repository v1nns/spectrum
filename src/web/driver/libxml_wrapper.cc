#include "web/driver/libxml_wrapper.h"

#include "util/logger.h"

namespace driver {

model::SongLyric LIBXMLWrapper::Parse(const std::string &data, const std::string &xpath) {
  // Parse HTML and create a DOM tree
  XmlDocGuard doc{htmlReadDoc((const xmlChar *)data.c_str(), nullptr, nullptr,
                              HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING),
                  &xmlFreeDoc};

  // Encapsulate raw libxml document in a libxml++ wrapper
  xmlNode *r = xmlDocGetRootElement(doc.get());
  auto root = std::make_unique<xmlpp::Element>(r);

  // Create structure to fill with lyrics
  model::SongLyric raw_lyric;

  try {
    // Find for XPath in parsed data
    auto elements = root->find(xpath);

    // If found, web scrap it to lyrics
    if (!elements.empty()) ScrapContent(elements.front(), raw_lyric);

  } catch (xmlpp::exception &err) {
    ERROR("Failed to scrap content using libXML++, error=", err.what());
  }

  return raw_lyric;
}

/* ********************************************************************************************** */

void LIBXMLWrapper::ScrapContent(const xmlpp::Node *node, model::SongLyric &lyric) {
  // Safely convert node to classes down along its inheritance hierarchy
  const auto node_text = dynamic_cast<const xmlpp::TextNode *>(node);
  const auto node_content = dynamic_cast<const xmlpp::ContentNode *>(node);

  // Consider only TextNode as the node that contains any lyric content
  if (node_text) lyric.push_back(node_text->get_content());

  if (!node_content && node) {
    // Recurse through child nodes to filter lyric
    for (const auto &child : node->get_children()) {
      ScrapContent(child, lyric);
    }
  }
}

}  // namespace driver
