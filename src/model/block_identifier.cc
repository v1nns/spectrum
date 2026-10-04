#include "model/block_identifier.h"

#include <iomanip>

namespace model {

static const char* to_chars(const BlockIdentifier& id) {
  switch (id) {
    case BlockIdentifier::Sidebar:
      return "Sidebar";
    case BlockIdentifier::FileInfo:
      return "FileInfo";
    case BlockIdentifier::MainContent:
      return "MainContent";
    case BlockIdentifier::MediaPlayer:
      return "MediaPlayer";
    case BlockIdentifier::None:
    default:
      return "None";
  }
}
/* ********************************************************************************************** */

//! BlockIdentifier pretty print
std::ostream& operator<<(std::ostream& out, const BlockIdentifier& id) {
  out << std::quoted(to_chars(id));
  return out;
}

}  // namespace model
