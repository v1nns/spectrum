#include "model/song.h"

#include <iomanip>
#include <sstream>
#include <string>

#include "util/formatter.h"

namespace model {

bool operator==(const Song::CurrentInformation& lhs, const Song::CurrentInformation& rhs) {
  return std::tie(lhs.state, lhs.position) == std::tie(rhs.state, rhs.position);
}

/* ********************************************************************************************** */

bool operator!=(const Song::CurrentInformation& lhs, const Song::CurrentInformation& rhs) {
  return !(lhs == rhs);
}

/* ********************************************************************************************** */

bool operator==(const Song& lhs, const Song& rhs) {
  return std::tie(lhs.filepath, lhs.artist, lhs.title, lhs.playlist, lhs.num_channels,
                  lhs.sample_rate, lhs.bit_rate, lhs.bit_depth, lhs.duration, lhs.curr_info) ==
         std::tie(rhs.filepath, rhs.artist, rhs.title, rhs.playlist, rhs.num_channels,
                  rhs.sample_rate, rhs.bit_rate, rhs.bit_depth, rhs.duration, rhs.curr_info);
}

/* ********************************************************************************************** */

bool operator!=(const Song& lhs, const Song& rhs) { return !(lhs == rhs); }

/* ********************************************************************************************** */

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

    case Song::MediaState::Finished:
      out << "Finished";
      break;
  }

  return out;
}

/* ********************************************************************************************** */

//! Song::CurrentInformation pretty print
std::ostream& operator<<(std::ostream& out, const Song::CurrentInformation& info) {
  out << "{state:" << info.state << " position:" << info.position << "}";
  return out;
}

/* ********************************************************************************************** */

//! Song pretty print
std::ostream& operator<<(std::ostream& out, const Song& s) {
  std::string artist = s.artist.empty() ? "<unknown>" : s.artist;
  std::string title = s.title.empty() ? "<unknown>" : s.title;

  out << "{index:" << (s.index ? std::to_string(*s.index) : "<none>")
      << " filename:" << (s.filepath.has_filename() ? s.filepath.filename() : "<none>")
      << " artist:" << artist << " title:" << title
      << " playlist:" << (s.playlist ? *s.playlist : "<none>") << " duration:" << s.duration
      << " sample_rate:" << s.sample_rate << " bit_rate:" << s.bit_rate
      << " bit_depth:" << s.bit_depth
      << " streaming_info:" << (s.stream_info ? to_string(*s.stream_info) : "<none>") << "}";
  return out;
}

/* ********************************************************************************************** */

bool Song::IsEmpty() const { return filepath.empty() && !stream_info.has_value(); }

/* ********************************************************************************************** */

std::string Song::GetTitle() const {
  std::string text;
  // Get title from streaming information
  if (stream_info.has_value()) text = !title.empty() ? title : stream_info->base_url;

  // Get artist and title from metadata information
  else if (!artist.empty() && !title.empty())
    text = artist + "-" + title;

  // Get title from metadata information
  else if (!title.empty())
    text = title;

  // As last resource, use filepath
  else
    text = filepath.filename().string();

  return text;
}

/* ********************************************************************************************** */

bool Song::Compare(const Song& other) const {
  // Attempt to use streaming information first
  if (stream_info.has_value() && other.stream_info.has_value())
    return stream_info->base_url == other.stream_info->base_url;

  // Otherwise, use filepath
  return !filepath.empty() && !other.filepath.empty() && filepath == other.filepath;
}

/* ********************************************************************************************** */

std::string to_string(const Song& arg) {
  bool is_empty = arg.IsEmpty();

  std::string filename = arg.filepath.empty() ? "<Empty>" : arg.filepath.filename();

  std::string artist = is_empty ? "<Empty>" : arg.artist.empty() ? "<Unknown>" : arg.artist;
  std::string title = is_empty ? "<Empty>" : arg.title.empty() ? "<Unknown>" : arg.title;

  std::string channels = is_empty ? "<Empty>" : std::to_string(arg.num_channels);
  std::string sample_rate = is_empty ? "<Empty>" : util::format_with_prefix(arg.sample_rate, "Hz");
  std::string bit_rate = is_empty ? "<Empty>" : util::format_with_prefix(arg.bit_rate, "bps");
  std::string bit_depth = is_empty ? "<Empty>" : util::format_with_prefix(arg.bit_depth, "bits");
  std::string duration = is_empty ? "<Empty>" : util::format_with_prefix(arg.duration, "sec");

  std::ostringstream ss;

  ss << "Filename: " << filename << std::endl;
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
