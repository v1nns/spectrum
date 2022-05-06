#include "model/song.h"

namespace model {

Song::Song(const std::string& full_path)
    : filename_(full_path), file_(filename_, std::ios::binary) {}

/* ********************************************************************************************** */

std::ostream& operator<<(std::ostream& os, const AudioData& arg) {
  // TODO: improve this
  std::string bit_rate;
  if (arg.bit_rate > 1000) {
    bit_rate = std::to_string(arg.bit_rate / 1000) + " kbps";
  } else {
    bit_rate = std::to_string(arg.bit_rate) + " bps";
  }

  os << "File format: " << arg.file_format << ".";
  os << "Channels: " << arg.num_channels << ".";
  os << "Sample rate: " << arg.sample_rate << ".";
  os << "Bit rate: " << bit_rate << ".";
  os << "Bits per sample: " << arg.bit_depth << ".";
  return os;
}

/* ********************************************************************************************** */

std::string to_string(const AudioData& arg) {
  std::ostringstream ss;
  ss << arg;
  return std::move(ss).str();
}

}  // namespace model