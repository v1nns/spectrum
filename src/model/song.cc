#include "model/song.h"

#include <math.h>

#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

namespace model {

bool Song::CurrentInformation::operator==(const Song::CurrentInformation& other) const {
  return std::tie(state, position) == std::tie(other.state, position);
}

bool Song::CurrentInformation::operator!=(const Song::CurrentInformation& other) const {
  return !operator==(other);
}

bool Song::operator==(const Song& other) const {
  return std::tie(filepath, artist, title, num_channels, sample_rate, bit_rate, bit_depth, duration,
                  curr_info) == std::tie(other.filepath, other.artist, other.title,
                                         other.num_channels, other.sample_rate, other.bit_rate,
                                         other.bit_depth, other.duration, other.curr_info);
}

bool Song::operator!=(const Song& other) const { return !operator==(other); }

//! Song::MediaState pretty print
std::ostream& operator<<(std::ostream& out, const Song::MediaState& state) {
  switch (state) {
    case Song::MediaState::Empty:
      out << "Empty";
      break;

    case Song::MediaState::Play:
      out << "Play";
      break;

    case Song::MediaState::Pause:
      out << "Pause";
      break;

    case Song::MediaState::Stop:
      out << "Stop";
      break;
  }

  return out;
}

//! Song::CurrentInformation pretty print
std::ostream& operator<<(std::ostream& out, const Song::CurrentInformation& info) {
  out << "{state:" << info.state << " position:" << info.position << "}";
  return out;
}

//! Song pretty print
std::ostream& operator<<(std::ostream& out, const Song& s) {
  std::string artist = s.artist.empty() ? "<unknown>" : s.artist;
  std::string title = s.title.empty() ? "<unknown>" : s.title;

  out << "{artist:" << artist << " title:" << title << " duration:" << s.duration
      << " sample_rate:" << s.sample_rate << " bit_rate:" << s.bit_rate
      << " bit_depth:" << s.bit_depth << "}";
  return out;
}

/* ********************************************************************************************** */

using Prefix = std::pair<int, std::string_view>;
using PrefixArray = std::array<Prefix, 4>;

static constexpr PrefixArray kPrefixes{{
    {0, ""},
    {3, "k"},
    {6, "M"},
    {9, "G"},
}};

/**
 * @brief Format value as string using metric prefix (from International System of Units)
 *
 * @tparam T Value type
 * @param value Value
 * @param unit Custom unit to concatenate on string
 * @return Formatted string
 */
template <typename T>
std::string format_with_prefix(T value, std::string unit) {
  std::ostringstream ss;

  if (value == 0) {
    ss << "0 " << unit;
    return std::move(ss).str();
  }

  int magnitude = log(value) / log(10);

  PrefixArray::const_reverse_iterator rit;
  for (rit = kPrefixes.rbegin(); rit < kPrefixes.rend(); ++rit) {
    if (magnitude >= rit->first) break;
  }

  ss << (value / std::pow(10, rit->first)) << " " << rit->second << unit;
  return std::move(ss).str();
}

/* ********************************************************************************************** */

std::string to_string(const Song& arg) {
  bool is_empty = arg.filepath.empty() ? true : false;

  std::string artist = is_empty ? "<Empty>" : arg.artist.empty() ? "<Unknown>" : arg.artist;
  std::string title = is_empty ? "<Empty>" : arg.title.empty() ? "<Unknown>" : arg.title;

  std::string channels = is_empty ? "<Empty>" : std::to_string(arg.num_channels);
  std::string sample_rate = is_empty ? "<Empty>" : format_with_prefix(arg.sample_rate, "Hz");
  std::string bit_rate = is_empty ? "<Empty>" : format_with_prefix(arg.bit_rate, "bps");
  std::string bit_depth = is_empty ? "<Empty>" : format_with_prefix(arg.bit_depth, "bits");
  std::string duration = is_empty ? "<Empty>" : format_with_prefix(arg.duration, "sec");

  std::ostringstream ss;

  //   oss << "Filepath: " << arg.filepath << std::endl;
  ss << "Artist: " << artist << std::endl;
  ss << "Title: " << title << std::endl;
  ss << "Channels: " << channels << std::endl;
  ss << "Sample rate: " << sample_rate << std::endl;
  ss << "Bit rate: " << bit_rate << std::endl;
  ss << "Bits per sample: " << bit_depth << std::endl;
  ss << "Duration: " << duration << std::endl;

  return std::move(ss).str();
}

/* ********************************************************************************************** */

std::string time_to_string(const uint32_t& arg) {
  const int hours = arg / 3600;
  const int minutes = (arg - (hours * 3600)) / 60;
  const int seconds = arg - (hours * 3600) - (minutes * 60);

  std::ostringstream ss;

  if (hours > 0) ss << std::setw(2) << std::setfill('0') << hours << ":";
  ss << std::setw(2) << std::setfill('0') << minutes << ":";
  ss << std::setw(2) << std::setfill('0') << seconds;

  return std::move(ss).str();
}

}  // namespace model
