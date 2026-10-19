#include "audio/lyric/search_config.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <regex>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"
#include "util/formatter.h"
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

//! Parse lyrics in LRC format, e.g. "[00:12.34] line", into lines sorted by timestamp
model::SyncedLyric ParseLrc(const std::string& text) {
  // Timestamp as [minutes:seconds] with optional fraction of second, e.g. [01:02.34] or [01:02:345]
  static const std::regex kTimestamp(R"(\[(\d{1,4}):(\d{1,2})(?:[.:](\d{1,3}))?\])");

  // Offset (in milliseconds) to apply on every timestamp, e.g. [offset:+500] or [offset:-250]
  static const std::regex kOffset(R"(\[offset:\s*([+-]?\d{1,6})\s*\])", std::regex::icase);

  static constexpr uint32_t kMillisecondsPerSecond = 1000;
  static constexpr uint32_t kSecondsPerMinute = 60;
  static constexpr std::size_t kFractionDigits = 3;  //!< Digits from fraction in milliseconds

  // Positive offset shows lyrics earlier (it is subtracted from timestamps)
  int64_t offset = 0;
  if (std::smatch match; std::regex_search(text, match, kOffset)) {
    offset = std::stoll(match[1].str());
  }

  model::SyncedLyric lyric;
  std::istringstream input{text};

  for (std::string line; std::getline(input, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();

    // A line may start with more than one timestamp, when it is sung more than once
    std::vector<uint32_t> timestamps;
    std::smatch match;
    auto begin = line.cbegin();

    while (std::regex_search(begin, line.cend(), match, kTimestamp,
                             std::regex_constants::match_continuous)) {
      // Fraction is padded to milliseconds, e.g. ".5" is 500 and ".34" is 340
      std::string fraction = match[3].str();
      fraction.resize(kFractionDigits, '0');

      const auto minutes = static_cast<uint32_t>(std::stoul(match[1].str()));
      const auto seconds = static_cast<uint32_t>(std::stoul(match[2].str()));
      const auto milliseconds = static_cast<uint32_t>(std::stoul(fraction));

      timestamps.push_back((minutes * kSecondsPerMinute + seconds) * kMillisecondsPerSecond +
                           milliseconds);

      begin = match[0].second;
    }

    // Metadata tags like [ar:Artist] or [offset:100] do not have a timestamp
    if (timestamps.empty()) continue;

    // Empty text is kept, as it marks an instrumental gap
    std::string content = util::trim(std::string(begin, line.cend()));
    for (uint32_t timestamp : timestamps) {
      lyric.push_back({static_cast<uint32_t>(std::max<int64_t>(0, timestamp - offset)), content});
    }
  }

  std::stable_sort(lyric.begin(), lyric.end(),
                   [](const auto& lhs, const auto& rhs) { return lhs.timestamp < rhs.timestamp; });

  return lyric;
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

std::string LRCLIB::FormatSearchUrl(const std::string& artist, const std::string& name,
                                    uint32_t duration) const {
  std::string url =
      url_ + "?artist_name=" + EncodeQuery(artist) + "&track_name=" + EncodeQuery(name);
  if (duration > 0) url += "&duration=" + std::to_string(duration);

  return url;
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

model::SyncedLyric LRCLIB::ExtractSyncedLyrics(const std::string& content) const {
  static constexpr const char* kLyricsKey = "syncedLyrics";  //!< Lyrics with timestamps

  nlohmann::json info = nlohmann::json::parse(content, nullptr, /*allow_exceptions=*/false);
  if (!info.is_object()) return model::SyncedLyric{};

  // Field is null when search engine has only plain lyrics (or for instrumental songs)
  auto lyrics = info.find(kLyricsKey);
  if (lyrics == info.end() || !lyrics->is_string()) return model::SyncedLyric{};

  return ParseLrc(lyrics->get<std::string>());
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

std::string AZLyrics::FormatSearchUrl(const std::string& artist, const std::string& name,
                                      uint32_t) const {
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
