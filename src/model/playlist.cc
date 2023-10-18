#include "model/playlist.h"

namespace model {

bool Playlist::operator==(const Playlist& other) const {
  return std::tie(name, songs) == std::tie(other.name, other.songs);
}

/* ********************************************************************************************** */

bool Playlist::operator!=(const Playlist& other) const { return !operator==(other); }

/* ********************************************************************************************** */

std::ostream& operator<<(std::ostream& out, const Playlist& p) {
  out << "{playlist:" << std::quoted(p.name) << " songs:" << p.songs.size() << "}";
  return out;
}

/* ********************************************************************************************** */

Song Playlist::PopFront() {
  auto song = songs.front();
  songs.pop_front();

  return song;
}

}  // namespace model
