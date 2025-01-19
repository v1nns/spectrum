#include "model/playlist_operation.h"

namespace model {

std::string PlaylistOperation::GetActionName(const PlaylistOperation& playlist) {
  using Operation = PlaylistOperation::Operation;

  switch (playlist.action) {
    case Operation::None:
      return "None";
    case Operation::Create:
      return "Create";
    case Operation::Modify:
      return "Modify";
    default:
      break;
  }

  return "Unknown";
}

/* ********************************************************************************************** */

//! PlaylistOperation pretty print
std::ostream& operator<<(std::ostream& out, const PlaylistOperation& p) {
  out << "{action: " << PlaylistOperation::GetActionName(p) << ", playlist: ";

  if (p.playlist.has_value())
    operator<<(out, p.playlist.value());
  else
    out << "{empty}";

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
