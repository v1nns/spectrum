#include "model/stream_info.h"

#include <iomanip>
#include <sstream>

namespace model {

bool operator==(const StreamInfo& lhs, const StreamInfo& rhs) {
  return std::tie(lhs.codec, lhs.extension, lhs.filesize, lhs.description, lhs.base_url,
                  lhs.streaming_url, lhs.http_header) ==
         std::tie(rhs.codec, rhs.extension, rhs.filesize, rhs.description, rhs.base_url,
                  rhs.streaming_url, rhs.http_header);
}

/* ********************************************************************************************** */

bool operator!=(const StreamInfo& lhs, const StreamInfo& rhs) { return !(lhs == rhs); }

/* ********************************************************************************************** */

//! StreamInfo pretty print
std::ostream& operator<<(std::ostream& out, const StreamInfo& s) {
  out << "{codec:" << std::quoted(s.codec) << ", extension:" << std::quoted(s.extension)
      << ", filesize:" << s.filesize << ", description:" << std::quoted(s.description)
      << ", base url:" << std::quoted(s.base_url) << "}";

  // Do not print streaming URL nor HTTP headers

  return out;
}

/* ********************************************************************************************** */

std::string to_string(const StreamInfo& arg) {
  std::ostringstream ss;

  // TODO: think about this
  ss << arg;

  return std::move(ss).str();
}

}  // namespace model
