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
    case Operation::Delete:
      return "Delete";
  }
}

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

}  // namespace model
