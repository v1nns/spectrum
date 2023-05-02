#include "audio/lyric/search_config.h"

#include <cctype>
#include <regex>

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

SongLyric Google::FormatLyrics(const SongLyric &raw) const {
  std::string::size_type pos = 0;
  std::string::size_type prev = 0;

  // TODO: if(raw.size() != 1) LOG ERROR
  const auto &content = raw.front();
  SongLyric lyric;

  // Split into paragraphs
  while ((pos = content.find("\n\n", prev)) != std::string::npos) {
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

SongLyric AZLyrics::FormatLyrics(const SongLyric &raw) const {
  SongLyric lyric;
  std::string paragraph;

  for (const auto &line : raw) {
    // first line
    if (line == "\r\n") continue;

    // newline means paragraph is finished
    if (line == "\n") {
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
    paragraph.pop_back();  // Remove newline
    lyric.push_back(paragraph);
  }

  return lyric;
}

}  // namespace lyric