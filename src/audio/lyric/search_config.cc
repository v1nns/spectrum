#include "audio/lyric/search_config.h"

#include <cctype>
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

std::string Google::FormatSearchUrl(const std::string &artist, const std::string &name) const {
  std::string raw_url = url_ + artist + "+" + name;
  return std::regex_replace(raw_url, std::regex(" "), "+");
}

/* ********************************************************************************************** */

model::SongLyric Google::FormatLyrics(const model::SongLyric &raw) const {
  std::string::size_type pos = 0;
  std::string::size_type prev = 0;
  model::SongLyric lyric;

  if (raw.size() != 1) {
    ERROR("Received more raw data than expected");
    return lyric;
  }

  const auto &content = raw.front();

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

std::string AZLyrics::FormatSearchUrl(const std::string &artist, const std::string &name) const {
  std::string formatted_url = url_ + artist + "/" + name + ".html";

  // Transform string into lowercase
  std::transform(formatted_url.begin(), formatted_url.end(), formatted_url.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  // Erase whitespaces
  formatted_url.erase(std::remove_if(formatted_url.begin(), formatted_url.end(), ::isspace),
                      formatted_url.end());

  return formatted_url;
}

/* ********************************************************************************************** */

model::SongLyric AZLyrics::FormatLyrics(const model::SongLyric &raw) const {
  model::SongLyric lyric;
  std::string paragraph;

  for (const auto &line : raw) {
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
