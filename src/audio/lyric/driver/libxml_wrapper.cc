#include "audio/lyric/driver/libxml_wrapper.h"

namespace driver {

lyric::SongLyric LIBXMLWrapper::Parse(const std::string &data, const std::string &xpath) {
  // Parse HTML and create a DOM tree
  XmlDocGuard doc{htmlReadDoc((xmlChar *)data.c_str(), NULL, NULL,
                              HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING),
                  &xmlFreeDoc};

  // Encapsulate raw libxml document in a libxml++ wrapper
  xmlNode *r = xmlDocGetRootElement(doc.get());
  ElementGuard root = std::make_unique<xmlpp::Element>(r);

  // Create structure to fill with lyrics
  lyric::SongLyric raw_lyric;

  try {
    // Find for XPath in parsed data
    auto elements = root->find(xpath);

    // If found, web scrap it to lyrics
    if (!elements.empty()) ScrapContent(elements.front(), raw_lyric);

  } catch (xmlpp::exception &err) {
    // TODO: log err.what()
  }

  return raw_lyric;
}

/* ********************************************************************************************** */

void LIBXMLWrapper::ScrapContent(const xmlpp::Node *node, lyric::SongLyric &lyric) {
  // Safely convert node to classes down along its inheritance hierarchy
  const auto node_text = dynamic_cast<const xmlpp::TextNode *>(node);
  const auto node_content = dynamic_cast<const xmlpp::ContentNode *>(node);

  // Considering only the TextNode as node that contains lyric content
  if (node_text) {
    lyric.push_back(node_text->get_content());
  }

  if (!node_content) {
    // Recurse through child nodes to filter lyric
    for (const auto &child : node->get_children()) {
      ScrapContent(child, lyric);
    }
  }
}

}  // namespace driver