#include "model/playlist.h"

namespace model {

std::ostream& operator<<(std::ostream& out, const Playlist& p) {
  out << "{id:" << p.index << ", playlist:" << std::quoted(p.name) << ", songs:" << p.songs.size()
      << "}";
  return out;
}

/* ********************************************************************************************** */

bool operator==(const Playlist& lhs, const Playlist& rhs) {
  return std::tie(lhs.index, lhs.name, lhs.songs) == std::tie(rhs.index, rhs.name, rhs.songs);
}

/* ********************************************************************************************** */

bool operator!=(const Playlist& lhs, const Playlist& rhs) { return !(lhs == rhs); }

/* ********************************************************************************************** */

Song Playlist::PopFront() {
  auto song = songs.front();
  songs.pop_front();

  return song;
}

/* ********************************************************************************************** */

void PrintTo(const Playlist& p, std::ostream* os) {
  *os << "{id:" << p.index << ", playlist:" << std::quoted(p.name);

  *os << ", songs:{";

  std::deque<Song>::const_iterator i, j;
  for (i = p.songs.begin(), j = --p.songs.end(); i != j; ++i) {
    *os << std::quoted(i->GetTitle()) << ",";
  }

  *os << j->filepath;
  *os << "}}";
}

}  // namespace model
