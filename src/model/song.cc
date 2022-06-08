#include "model/song.h"

namespace model {

std::ostream& operator<<(std::ostream& os, const Song& arg) {
  // TODO: improve this
  std::string bit_rate;
  if (arg.bit_rate > 1000) {
    bit_rate = std::to_string(arg.bit_rate / 1000) + " kbps";
  } else {
    bit_rate = std::to_string(arg.bit_rate) + " bps";
  }

  os << "Artist: " << arg.artist << ".";
  os << "Title: " << arg.title << ".";
  os << "Channels: " << arg.num_channels << ".";
  os << "Sample rate: " << arg.sample_rate << ".";
  os << "Bit rate: " << bit_rate << ".";
  os << "Bits per sample: " << arg.bit_depth << ".";
  return os;
}

/* ********************************************************************************************** */

std::string to_string(const Song& arg) {
  std::ostringstream ss;
  ss << arg;
  return std::move(ss).str();
}

}  // namespace model