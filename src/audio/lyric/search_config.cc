#include "audio/lyric/search_config.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <regex>

#include "util/logger.h"

namespace lyric {

/* ---------------------------------------------------------------------------------------------- */
/*                           Create config with available search engines                          */
/* ---------------------------------------------------------------------------------------------- */

Config SearchConfig::Create() {
  return Config{
      std::make_unique<Google>(),
      std::make_unique<AZLyrics>(),
  };
}

/* ---------------------------------------------------------------------------------------------- */
/*                                             Google                                             */
/* ---------------------------------------------------------------------------------------------- */

std::string Google::FormatSearchUrl(const std::string& artist, const std::string& name) const {
  std::string formatted_url = url_;

  // Encode search terms as URL query (otherwise characters like "&" or "#" would break it)
  for (unsigned char c : artist + " " + name) {
    if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
      formatted_url += static_cast<char>(c);
    } else if (c == ' ') {
      formatted_url += '+';
    } else {
      char encoded[4];
      std::snprintf(encoded, sizeof(encoded), "%%%02X", c);
      formatted_url += encoded;
    }
  }

  return formatted_url;
}

/* ********************************************************************************************** */

model::SongLyric Google::FormatLyrics(const model::SongLyric& raw) const {
  std::string::size_type pos = 0;
  std::string::size_type prev = 0;
  model::SongLyric lyric;

  if (raw.size() != 1) {
    ERROR("Received more raw data than expected");
    return lyric;
  }

  const auto& content = raw.front();

  // Split into paragraphs
  while ((pos = content.find("\n\n", prev)) != std::string::npos) {
    pos += 1;  // To avoid having a \n in the beginning
    lyric.push_back(content.substr(prev, pos - prev));
    prev = pos + 1;
  }

  lyric.push_back(content.substr(prev));
  return lyric;
}

/* ---------------------------------------------------------------------------------------------- */
/*                                            AZLyrics                                            */
/* ---------------------------------------------------------------------------------------------- */

std::string AZLyrics::FormatSearchUrl(const std::string& artist, const std::string& name) const {
  // AZLyrics URL uses only lowercase letters and digits from artist and song name
  auto format = [](const std::string& input) {
    std::string output;

    for (unsigned char c : input) {
      if (std::isalnum(c)) output += static_cast<char>(std::tolower(c));
    }

    return output;
  };

  return url_ + format(artist) + "/" + format(name) + ".html";
}

/* ********************************************************************************************** */

model::SongLyric AZLyrics::FormatLyrics(const model::SongLyric& raw) const {
  model::SongLyric lyric;
  std::string paragraph;

  for (const auto& line : raw) {
    // first line
    if (line == "\r\n") continue;

    // newline means paragraph is finished
    if (line == "\n" && !paragraph.empty()) {
      lyric.push_back(paragraph);
      paragraph.clear();
      continue;
    }

    // Otherwise it is a common line, remove any carriage return or line feed characters from it
    std::string tmp = std::regex_replace(line, std::regex("[\r\n]+"), "");
    if (tmp.size() > 0) paragraph.append(tmp + "\n");
  }

  // We may not receive the last newline, append last paragraph if not empty
  if (!paragraph.empty()) {
    lyric.push_back(paragraph);
  }

  return lyric;
}

}  // namespace lyric
