#include "audio/lyric/search_config.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <regex>
#include <sstream>
#include <string_view>
#include <utility>

#include "nlohmann/json.hpp"
#include "util/logger.h"

namespace lyric {

namespace {

//! Encode text as URL query (otherwise characters like "&" or "#" would break it)
std::string EncodeQuery(const std::string& text) {
  std::string encoded;

  for (unsigned char c : text) {
    if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
      encoded += static_cast<char>(c);
    } else if (c == ' ') {
      encoded += '+';
    } else {
      char hex[4];
      std::snprintf(hex, sizeof(hex), "%%%02X", c);
      encoded += hex;
    }
  }

  return encoded;
}

//! Article in the beginning of artist name (lowercase, including the space after it)
constexpr std::string_view kArticle = "the ";

//! Accented letters (encoded as UTF-8) and their matching plain letter
struct Accent {
  std::string_view letters;  //!< Accented letters
  char plain;                //!< Letter without accent (lowercase)
};

constexpr std::array<Accent, 8> kAccents{{
    {"àáâãäåÀÁÂÃÄÅ", 'a'},
    {"çÇ", 'c'},
    {"èéêëÈÉÊË", 'e'},
    {"ìíîïÌÍÎÏ", 'i'},
    {"ñÑ", 'n'},
    {"òóôõöøÒÓÔÕÖØ", 'o'},
    {"ùúûüÙÚÛÜ", 'u'},
    {"ýÿÝ", 'y'},
}};

constexpr std::string_view::size_type kAccentSize = 2;  //!< Bytes used by a single accented letter
constexpr char kNoAccent = '\0';                        //!< Text is not an accented letter

//! Get plain letter for the given accented letter (or kNoAccent if it is not one of them)
char RemoveAccent(std::string_view letter) {
  if (letter.size() != kAccentSize) return kNoAccent;

  for (const auto& accent : kAccents) {
    // Match must be aligned to a whole letter, and not to bytes from two different letters
    if (auto pos = accent.letters.find(letter);
        pos != std::string_view::npos && pos % kAccentSize == 0) {
      return accent.plain;
    }
  }

  return kNoAccent;
}

}  // namespace

/* ---------------------------------------------------------------------------------------------- */
/*                           Create config with available search engines                          */
/* ---------------------------------------------------------------------------------------------- */

Config SearchConfig::Create() {
  return Config{
      std::make_unique<LRCLIB>(),
      std::make_unique<AZLyrics>(),
  };
}

/* ---------------------------------------------------------------------------------------------- */
/*                                             LRCLIB                                             */
/* ---------------------------------------------------------------------------------------------- */

std::string LRCLIB::FormatSearchUrl(const std::string& artist, const std::string& name) const {
  return url_ + "?artist_name=" + EncodeQuery(artist) + "&track_name=" + EncodeQuery(name);
}

/* ********************************************************************************************** */

model::SongLyric LRCLIB::ExtractLyrics(const std::string& content, web::HtmlParser&) const {
  static constexpr const char* kLyricsKey = "plainLyrics";  //!< Lyrics without timestamps

  nlohmann::json info = nlohmann::json::parse(content, nullptr, /*allow_exceptions=*/false);

  if (!info.is_object()) {
    WARN("Could not parse content from search engine=", kEngineName);
    return model::SongLyric{};
  }

  // Field is null for instrumental songs
  auto lyrics = info.find(kLyricsKey);
  if (lyrics == info.end() || !lyrics->is_string()) return model::SongLyric{};

  std::string text = lyrics->get<std::string>();
  return text.empty() ? model::SongLyric{} : model::SongLyric{std::move(text)};
}

/* ********************************************************************************************** */

model::SongLyric LRCLIB::FormatLyrics(const model::SongLyric& raw) const {
  model::SongLyric lyric;
  std::string paragraph;

  for (const auto& content : raw) {
    std::istringstream input{content};

    for (std::string line; std::getline(input, line);) {
      if (!line.empty() && line.back() == '\r') line.pop_back();

      // Empty line means paragraph is finished
      if (line.empty()) {
        if (!paragraph.empty()) lyric.push_back(paragraph);
        paragraph.clear();
        continue;
      }

      paragraph.append(line + "\n");
    }
  }

  // Append last paragraph, as content does not end with an empty line
  if (!paragraph.empty()) lyric.push_back(paragraph);

  return lyric;
}

/* ---------------------------------------------------------------------------------------------- */
/*                                            AZLyrics                                            */
/* ---------------------------------------------------------------------------------------------- */

std::string AZLyrics::FormatSearchUrl(const std::string& artist, const std::string& name) const {
  // AZLyrics URL uses only lowercase letters and digits from artist and song name
  auto format = [](std::string_view input) {
    std::string output;

    for (std::string_view::size_type i = 0; i < input.size(); i++) {
      // Accented letter is replaced by its plain letter, instead of being discarded
      if (char plain = RemoveAccent(input.substr(i, kAccentSize)); plain != kNoAccent) {
        output += plain;
        i += kAccentSize - 1;
        continue;
      }

      if (auto c = static_cast<unsigned char>(input[i]); std::isalnum(c)) {
        output += static_cast<char>(std::tolower(c));
      }
    }

    return output;
  };

  // Article in the beginning of artist name is not part of URL, e.g. "The Beatles" is "beatles"
  std::string_view artist_name = artist;

  if (artist_name.size() > kArticle.size() &&
      std::equal(kArticle.begin(), kArticle.end(), artist_name.begin(), [](char lhs, char rhs) {
        return lhs == static_cast<char>(std::tolower(static_cast<unsigned char>(rhs)));
      })) {
    artist_name.remove_prefix(kArticle.size());
  }

  return url_ + format(artist_name) + "/" + format(name) + ".html";
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
