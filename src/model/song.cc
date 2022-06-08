#include "model/song.h"

#include <string>

namespace model {

std::ostream& operator<<(std::ostream& os, const Song& entry) {
  return os << entry.filepath << "|" << entry.artist << "|" << entry.title << "|"
            << entry.num_channels << "|" << entry.sample_rate << "|" << entry.bit_rate << "|"
            << entry.bit_depth << "|" << entry.duration << "|";
}

/* ********************************************************************************************** */

std::istream& operator>>(std::istream& is, Song& entry) {
  std::string token;
  std::getline(is, token, '|');
  entry.filepath = token;

  std::getline(is, token, '|');
  entry.artist = token;

  std::getline(is, token, '|');
  entry.title = token;

  std::getline(is, token, '|');
  entry.num_channels = std::atoi(token.c_str());

  std::getline(is, token, '|');
  entry.sample_rate = std::atoi(token.c_str());

  std::getline(is, token, '|');
  entry.bit_rate = std::atoi(token.c_str());

  std::getline(is, token, '|');
  entry.bit_depth = std::atoi(token.c_str());

  std::getline(is, token, '|');
  entry.duration = std::atoi(token.c_str());

  return is;
}

/* ********************************************************************************************** */

// std::ostream& operator<<(std::ostream& oss, const Song& arg) {
//   return oss;
// }

/* ********************************************************************************************** */

std::string to_string(const Song& arg) {
  std::ostringstream ss;

  //   // TODO: improve this
  //   std::string bit_rate;
  //   if (arg.bit_rate > 1000) {
  //     bit_rate = std::to_string(arg.bit_rate / 1000) + " kbps";
  //   } else {
  //     bit_rate = std::to_string(arg.bit_rate) + " bps";
  //   }

  std::string artist = arg.artist.empty() ? "<Unknown>" : arg.artist;
  std::string title = arg.title.empty() ? "<Unknown>" : arg.title;

  //   oss << "Filepath: " << arg.filepath << std::endl;
  ss << "Artist: " << artist << std::endl;
  ss << "Title: " << title << std::endl;
  ss << "Channels: " << arg.num_channels << std::endl;
  ss << "Sample rate: " << arg.sample_rate << std::endl;
  ss << "Bit rate: " << arg.bit_rate << std::endl;
  ss << "Bits per sample: " << arg.bit_depth << std::endl;
  ss << "Duration (seconds): " << arg.duration << std::endl;

  return std::move(ss).str();
}

}  // namespace model