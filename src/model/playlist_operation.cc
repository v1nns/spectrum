#include "model/playlist_operation.h"

#include <iomanip>

namespace model {

static const char* to_chars(const PlaylistOperation::Operation action) {
  using Operation = PlaylistOperation::Operation;

  switch (action) {
    case Operation::None:
      return "None";
    case Operation::Create:
      return "Create";
    case Operation::Modify:
      return "Modify";
    default:
      return "Unknown";
  }
}

/* ********************************************************************************************** */

//! PlaylistOperation pretty print
std::ostream& operator<<(std::ostream& out, const PlaylistOperation& p) {
  out << "{action:" << std::quoted(to_chars(p.action));
  out << ", playlist:";

  if (p.playlist.has_value())
    operator<<(out, *p.playlist);
  else
    out << "{}";

  out << "}";
  return out;
}

/* ********************************************************************************************** */

bool operator==(const PlaylistOperation& lhs, const PlaylistOperation& rhs) {
  return std::tie(lhs.action, lhs.playlist) == std::tie(rhs.action, rhs.playlist);
}

/* ********************************************************************************************** */

bool operator!=(const PlaylistOperation& lhs, const PlaylistOperation& rhs) {
  return !(lhs == rhs);
}

}  // namespace model
