#include "model/playlist.h"

namespace model {

bool Playlist::operator==(const Playlist& other) const {
  return std::tie(index, name, songs) == std::tie(other.index, other.name, other.songs);
}

/* ********************************************************************************************** */

bool Playlist::operator!=(const Playlist& other) const { return !operator==(other); }

/* ********************************************************************************************** */

std::ostream& operator<<(std::ostream& out, const Playlist& p) {
  out << "{id:" << p.index << " playlist:" << std::quoted(p.name) << " songs:" << p.songs.size()
      << "}";
  return out;
}

/* ********************************************************************************************** */

Song Playlist::PopFront() {
  auto song = songs.front();
  songs.pop_front();

  return song;
}

/* ********************************************************************************************** */

void PrintTo(const Playlist& p, std::ostream* os) {
  *os << "{id:" << p.index << " playlist:" << std::quoted(p.name);

  *os << " songs:{";

  std::deque<Song>::const_iterator i, j;
  for (i = p.songs.begin(), j = --p.songs.end(); i != j; ++i) {
    *os << i->filepath << ",";
  }

  *os << j->filepath;
  *os << "} }";
}

}  // namespace model
