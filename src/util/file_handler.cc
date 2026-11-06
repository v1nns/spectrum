#include "util/file_handler.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <iomanip>
#include <set>
#include <string_view>
#include <system_error>
#include <type_traits>

#include "model/audio_filter.h"
#include "nlohmann/json.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "util/url.h"

namespace util {

namespace internal {

static constexpr std::string_view kPlaylistsFile = "playlists.json";  //!< File with playlists
static constexpr std::string_view kSettingsFile = "settings.json";    //!< File with settings
static constexpr std::string_view kStatsFile = "stats.json";          //!< File with listening stats

//! Directory (inside home) used by default for files saved by user
static constexpr std::string_view kConfigDirectory = "/.config/spectrum";

//! Directory (inside home) used for log file, and by older versions for files saved by user
static constexpr std::string_view kCacheDirectory = "/.cache/spectrum";

//! Directory (inside home) used by default for state kept between runs (e.g. listening stats)
static constexpr std::string_view kStateDirectory = "/.local/state/spectrum";

//! Transform single character into lowercase
static void to_lower(char& c) { c = (char)std::tolower(c); }

/**
 * @brief Custom file sort algorithm
 * @param a Filename a
 * @param b Filename b
 * @return true if 'a' is alphabetically lesser than b, false otherwise
 */
static bool sort_files(const File& a, const File& b) {
  std::string lhs{a.filename()};
  std::string rhs{b.filename()};

  // Don't care if it is hidden (tried to make it similar to "ls" output)
  if (lhs.at(0) == '.') lhs.erase(0, 1);
  if (rhs.at(0) == '.') rhs.erase(0, 1);

  std::for_each(lhs.begin(), lhs.end(), to_lower);
  std::for_each(rhs.begin(), rhs.end(), to_lower);

  return lhs < rhs;
}

/**
 * @brief Fill value with unsigned number from JSON object, only when it has the expected type (so a
 * single bad value is ignored)
 * @param object JSON object with value
 * @param name Value name
 * @param value Value to fill (unchanged when missing or invalid)
 */
template <typename Value>
static void ReadUnsigned(const nlohmann::json& object, const char* name, Value& value) {
  if (auto it = object.find(name); it != object.end() && it->is_number_unsigned()) {
    value = it->get<Value>();
  }
}

/**
 * @brief Parse a single song from playlists file and append it to the given playlist
 * @param song JSON object with song data (throws nlohmann::json::exception if a field is invalid)
 * @param filepaths Files already added to playlist (to avoid including the same file twice)
 * @param playlist Playlist to append song
 */
static void ParseSong(const nlohmann::json& song, std::set<std::filesystem::path>& filepaths,
                      model::Playlist& playlist) {
  if (song.contains("path") && std::filesystem::exists(song["path"].get<std::string>())) {
    // Insert only if filepath is not duplicated
    if (auto [it, inserted] = filepaths.emplace(song["path"].get<std::string>()); inserted) {
      // Song from filepath
      playlist.songs.emplace_back(model::Song{
          .filepath = song["path"].get<std::string>(),
      });
    }

  } else if (song.contains("url") && util::IsYoutubeUrl(song["url"])) {
    // Song from URL
    playlist.songs.emplace_back(model::Song{
        .artist = song.contains("artist") ? util::filter_emoji(song["artist"]) : "",
        .title = song.contains("title") ? util::filter_emoji(song["title"]) : "",
        .stream_info =
            model::StreamInfo{
                .base_url = song["url"],
            },
    });
  }
}

/**
 * @brief Copy file to "<filepath>.bak" (replacing any older backup), so its content is not lost
 * when the original file is overwritten
 * @param filepath Full path to file
 */
static void BackupFile(const std::string& filepath) {
  static constexpr std::string_view kBackupSuffix = ".bak";

  std::string backup = filepath + std::string(kBackupSuffix);
  std::error_code error;

  std::filesystem::copy_file(filepath, backup, std::filesystem::copy_options::overwrite_existing,
                             error);

  if (error) {
    ERROR("Cannot create backup file=", std::quoted(backup), ", error=", error.message());
    return;
  }

  LOG("Created backup file=", std::quoted(backup));
}

/**
 * @brief Fill animation from its identifier. Older versions had animations without space between
 * bars, which are now the same animation with spacing disabled
 * @param id Animation identifier
 * @param settings Settings to fill (unchanged when identifier is unknown)
 */
static void ParseAnimation(const std::string& id, model::Settings& settings) {
  static constexpr std::string_view kNoSpaceSuffix = "-no-space";

  if (auto animation = model::GetAnimationFromId(id); animation) {
    settings.animation = animation;
    return;
  }

  if (id.size() > kNoSpaceSuffix.size() && id.substr(id.size() - kNoSpaceSuffix.size()) ==
                                               kNoSpaceSuffix) {
    if (auto animation = model::GetAnimationFromId(id.substr(0, id.size() - kNoSpaceSuffix.size()));
        animation && model::IsBarAnimation(*animation)) {
      settings.animation = animation;
      settings.bar_spacing = false;
    }
  }
}

/**
 * @brief Fill animation from the number saved by older versions (whose animations were in another
 * order, including the ones without space between bars)
 * @param value Animation number
 * @param settings Settings to fill (unchanged when number is unknown)
 */
static void ParseLegacyAnimation(int value, model::Settings& settings) {
  struct Legacy {
    model::BarAnimation animation = model::BarAnimation::HorizontalMirror;
    bool spacing = true;
  };

  static constexpr int kFirst = 11000;
  static constexpr std::array<Legacy, 10> kLegacy{{
      {model::BarAnimation::HorizontalMirror, true},
      {model::BarAnimation::VerticalMirror, true},
      {model::BarAnimation::Mono, true},
      {model::BarAnimation::HorizontalMirror, false},
      {model::BarAnimation::VerticalMirror, false},
      {model::BarAnimation::Mono, false},
      {model::BarAnimation::SpectrumLine, true},
      {model::BarAnimation::SpectrumLineMirror, true},
      {model::BarAnimation::SpectrumLineFilled, true},
      {model::BarAnimation::SpectrumLineFilledMirror, true},
  }};

  if (value < kFirst || value >= kFirst + static_cast<int>(kLegacy.size())) return;

  const auto& legacy = kLegacy.at(static_cast<size_t>(value - kFirst));
  settings.animation = legacy.animation;
  if (!legacy.spacing) settings.bar_spacing = false;
}

}  // namespace internal

/* ********************************************************************************************** */

std::string FileHandler::GetHome() const {
#ifdef _WIN32
  // On Windows, the home directory is typically in the USERPROFILE environment variable
  const char* home = std::getenv("USERPROFILE");
#else
  // On Unix-like systems, the home directory is in the HOME environment variable
  const char* home = std::getenv("HOME");
#endif

  return home ? std::string{home} : std::string{};
}

/* ********************************************************************************************** */

std::string FileHandler::GetConfigDirectory() const {
  // Relative path is not valid for this variable, so it is ignored (as stated by XDG specification)
  if (const char* config = std::getenv("XDG_CONFIG_HOME");
      config && std::filesystem::path{config}.is_absolute()) {
    return (std::filesystem::path{config} / "spectrum").string();
  }

  return GetHome() + std::string{internal::kConfigDirectory};
}

/* ********************************************************************************************** */

std::string FileHandler::GetPlaylistsPath() const {
  return GetConfigDirectory() + "/" + std::string{internal::kPlaylistsFile};
}

/* ********************************************************************************************** */

std::string FileHandler::GetSettingsPath() const {
  return GetConfigDirectory() + "/" + std::string{internal::kSettingsFile};
}

/* ********************************************************************************************** */

std::string FileHandler::GetLogPath() const {
  return GetHome() + std::string{internal::kCacheDirectory} + "/spectrum.log";
}

/* ********************************************************************************************** */

std::string FileHandler::GetStatsPath() const {
  // Relative path is not valid for this variable, so it is ignored (as stated by XDG specification)
  if (const char* state = std::getenv("XDG_STATE_HOME");
      state && std::filesystem::path{state}.is_absolute()) {
    return (std::filesystem::path{state} / "spectrum" / internal::kStatsFile).string();
  }

  return GetHome() + std::string{internal::kStateDirectory} + "/" +
         std::string{internal::kStatsFile};
}

/* ********************************************************************************************** */

void FileHandler::MigrateLegacyFiles() {
  const std::filesystem::path legacy_dir{GetHome() + std::string{internal::kCacheDirectory}};
  const std::filesystem::path config_dir{GetConfigDirectory()};

  for (const auto& filename : {internal::kPlaylistsFile, internal::kSettingsFile}) {
    const std::filesystem::path legacy = legacy_dir / filename;
    const std::filesystem::path current = config_dir / filename;
    std::error_code error;

    if (!std::filesystem::exists(legacy, error)) continue;

    if (std::filesystem::exists(current, error)) {
      WARN("File from older version was not moved, as there is a newer one, file=",
           std::quoted(legacy.string()));
      continue;
    }

    if (!CreateDirectory(config_dir.string(), error)) {
      ERROR("Cannot create directory=", std::quoted(config_dir.string()), ", error=", error);
      return;
    }

    // Copy and remove, as these directories may be on different filesystems (rename would fail)
    if (std::filesystem::copy_file(legacy, current, error); error) {
      ERROR("Cannot move file from older version, file=", std::quoted(legacy.string()),
            ", error=", error.message());
      continue;
    }

    std::filesystem::remove(legacy, error);
    INFO("Moved file from older version, from=", std::quoted(legacy.string()),
         " to=", std::quoted(current.string()));
  }
}

/* ********************************************************************************************** */

bool FileHandler::ListFiles(const std::filesystem::path& dir_path, Files& parsed_files) {
  Files tmp;

  try {
    // Add all files from the given directory
    for (auto const& entry : std::filesystem::directory_iterator(dir_path)) {
      tmp.emplace_back(entry);
    }
  } catch (std::exception& e) {
    ERROR("Cannot access directory, exception=", e.what());
    return false;
  }

  // Sort list alphabetically (case insensitive)
  std::sort(tmp.begin(), tmp.end(), internal::sort_files);

  // Add option to go back one level
  tmp.emplace(tmp.begin(), "..");

  // Update structure with parsed files
  parsed_files.swap(tmp);

  return true;
}

/* ********************************************************************************************** */

bool FileHandler::ParsePlaylists(model::Playlists& playlists) {
  std::string file_path{GetPlaylistsPath()};

  if (!std::filesystem::exists(file_path)) return false;

  nlohmann::json parsed;

  try {
    std::ifstream json(file_path);
    parsed = nlohmann::json::parse(json);
  } catch (const std::exception& e) {
    // Besides invalid JSON, file may not even be readable (e.g. it is a directory)
    ERROR("Cannot parse playlists file=", std::quoted(file_path), ", error=", e.what());
    internal::BackupFile(file_path);
    return false;
  }

  LOG("Found playlist file, start parsing it");
  if (!parsed.is_object() || !parsed.contains("playlists") || !parsed["playlists"].is_array()) {
    ERROR("Playlists file does not contain a list of playlists, file=", std::quoted(file_path));
    internal::BackupFile(file_path);
    return false;
  }

  model::Playlists tmp;
  bool skipped = false;  // Invalid entries are lost on next save, so keep a backup of the file

  // Parse all playlists (skipping only the invalid ones, so a single bad entry does not discard
  // all the others)
  for (auto& [_, playlist] : parsed["playlists"].items()) {
    if (!playlist.is_object() || !playlist.contains("name") || !playlist.contains("songs") ||
        !playlist["name"].is_string() || !playlist["songs"].is_array()) {
      WARN("Skipping playlist with missing or invalid name/songs, playlist=", playlist.dump());
      skipped = true;
      continue;
    }

    model::Playlist entry{.name = playlist["name"], .songs = {}};

    // To avoid including same song multiple times...
    std::set<std::filesystem::path> filepaths;

    // Parse all songs from a single playlist
    for (auto& [_, song] : playlist["songs"].items()) {
      try {
        internal::ParseSong(song, filepaths, entry);
      } catch (const nlohmann::json::exception& e) {
        WARN("Skipping invalid song=", song.dump(), " from playlist=", std::quoted(entry.name),
             ", error=", e.what());
        skipped = true;
      }
    }

    // Append playlist
    tmp.push_back(entry);
  }

  if (skipped) internal::BackupFile(file_path);

  INFO("Parsed ", tmp.size(), " playlists");
  playlists = std::move(tmp);
  return true;
}

/* ********************************************************************************************** */

bool FileHandler::SavePlaylists(const model::Playlists& playlists) {
  // Start by parsing c++ model structure into JSON structure
  // P.S.: lists are created as arrays, otherwise an empty one would be saved as null, which is
  // not accepted when file is parsed again
  nlohmann::json json_playlists = nlohmann::json::array();

  for (const auto& playlist : playlists) {
    nlohmann::json json_playlist;
    nlohmann::json json_songs = nlohmann::json::array();

    json_playlist["name"] = playlist.name;

    for (const auto& song : playlist.songs) {
      nlohmann::json json_song;

      // Common information
      if (!song.artist.empty()) json_song["artist"] = song.artist;
      if (!song.title.empty()) json_song["title"] = song.title;

      // Exclusive source to play song from
      if (!song.filepath.empty())
        json_song["path"] = song.filepath.string();
      else if (song.stream_info.has_value())
        json_song["url"] = song.stream_info->base_url;

      json_songs.push_back(json_song);
    }

    json_playlist["songs"] = json_songs;
    json_playlists.push_back(json_playlist);
  }

  nlohmann::json json_data;
  json_data["playlists"] = json_playlists;

  std::filesystem::path filepath{GetPlaylistsPath()};
  std::error_code error;

  // Check that parent directory exists
  if (!CreateDirectory(filepath.parent_path(), error)) {
    ERROR("Cannot create parent directory for cache file, error=", error);
    return false;
  }

  // Open the file in write mode
  std::ofstream out(filepath.string());

  if (!out.is_open()) {
    ERROR("Cannot open file for writing playlists");
    return false;
  }

  // Pretty print JSON data with indentation of 2 spaces
  out << std::setw(2) << json_data;

  if (out.fail()) {
    ERROR("Failed to write JSON");
    return false;
  }

  return true;
}

/* ********************************************************************************************** */

bool FileHandler::ParseSettings(model::Settings& settings) {
  std::string file_path{GetSettingsPath()};

  if (!std::filesystem::exists(file_path)) return false;

  nlohmann::json parsed;

  try {
    std::ifstream json(file_path);
    parsed = nlohmann::json::parse(json);
  } catch (const std::exception& e) {
    // Besides invalid JSON, file may not even be readable (e.g. it is a directory)
    ERROR("Cannot parse settings file=", std::quoted(file_path), ", error=", e.what());
    internal::BackupFile(file_path);
    return false;
  }

  if (!parsed.is_object()) {
    ERROR("Settings file does not contain an object, file=", std::quoted(file_path));
    internal::BackupFile(file_path);
    return false;
  }

  if (auto visualizer = parsed.find("visualizer");
      visualizer != parsed.end() && visualizer->is_object()) {
    // Animation is saved by its identifier, so check it is a known one
    if (auto animation = visualizer->find("animation");
        animation != visualizer->end() && animation->is_string()) {
      internal::ParseAnimation(animation->get<std::string>(), settings);
    } else if (animation != visualizer->end() && animation->is_number_integer()) {
      // It was saved as a number by older versions
      internal::ParseLegacyAnimation(animation->get<int>(), settings);
    }

    if (auto bar_width = visualizer->find("bar_width");
        bar_width != visualizer->end() && bar_width->is_number_integer()) {
      settings.bar_width = bar_width->get<int>();
    }

    // Options are saved by their identifiers, so check they are known ones
    if (auto style = visualizer->find("bar_style");
        style != visualizer->end() && style->is_string()) {
      settings.bar_style = model::GetBarStyleFromId(style->get<std::string>());
    }

    // Spacing chosen explicitly wins over the one from a legacy animation identifier
    if (auto spacing = visualizer->find("bar_spacing");
        spacing != visualizer->end() && spacing->is_boolean()) {
      settings.bar_spacing = spacing->get<bool>();
    }

    if (auto colors = visualizer->find("bar_colors");
        colors != visualizer->end() && colors->is_string()) {
      settings.bar_colors = model::GetBarColorsFromId(colors->get<std::string>());
    }

    if (auto peaks = visualizer->find("peaks"); peaks != visualizer->end() && peaks->is_boolean()) {
      settings.peaks = peaks->get<bool>();
    }

    if (auto hue = visualizer->find("hue_cycle"); hue != visualizer->end() && hue->is_string()) {
      settings.hue_cycle = model::GetHueCycleFromId(hue->get<std::string>());
    }
  }

  if (auto player = parsed.find("player"); player != parsed.end() && player->is_object()) {
    if (auto volume = player->find("volume");
        volume != player->end() && volume->is_number_integer()) {
      if (int value = volume->get<int>(); value >= 0 && value <= 100) settings.volume = value;
    }

    if (auto device = player->find("device"); device != player->end() && device->is_string()) {
      settings.device = device->get<std::string>();
    }

    // Repeat mode is saved by its name, so check it is a known one
    if (auto repeat = player->find("repeat"); repeat != player->end() && repeat->is_string()) {
      const auto name = repeat->get<std::string>();

      for (auto mode : {model::RepeatMode::Off, model::RepeatMode::All, model::RepeatMode::One}) {
        if (name == model::GetRepeatModeName(mode)) settings.repeat = mode;
      }
    }

    if (auto shuffle = player->find("shuffle");
        shuffle != player->end() && shuffle->is_boolean()) {
      settings.shuffle = shuffle->get<bool>();
    }
  }

  if (auto equalizer = parsed.find("equalizer");
      equalizer != parsed.end() && equalizer->is_object()) {
    if (auto preset = equalizer->find("preset");
        preset != equalizer->end() && preset->is_string()) {
      settings.equalizer_preset = preset->get<std::string>();
    }

    // There must be a gain for each frequency, all of them inside the limits
    if (auto custom = equalizer->find("custom");
        custom != equalizer->end() && custom->is_array() &&
        custom->size() == model::equalizer::kFiltersPerPreset &&
        std::all_of(custom->begin(), custom->end(), [](const nlohmann::json& gain) {
          return gain.is_number() && gain.get<double>() >= model::AudioFilter::kMinGain &&
                 gain.get<double>() <= model::AudioFilter::kMaxGain;
        })) {
      settings.equalizer_custom = custom->get<std::vector<double>>();
    }
  }

  if (auto interface = parsed.find("interface");
      interface != parsed.end() && interface->is_object()) {
    if (auto theme = interface->find("theme"); theme != interface->end() && theme->is_string()) {
      settings.theme = theme->get<std::string>();
    }
  }

  if (auto stream = parsed.find("stream"); stream != parsed.end() && stream->is_object()) {
    if (auto browser = stream->find("cookies_from_browser");
        browser != stream->end() && browser->is_string()) {
      settings.cookies_from_browser = browser->get<std::string>();
    }
  }

  LOG("Parsed settings from file=", std::quoted(file_path));
  return true;
}

/* ********************************************************************************************** */

bool FileHandler::SaveSettings(const model::Settings& settings) {
  std::filesystem::path filepath{GetSettingsPath()};

  // Settings are saved by different parts of the interface, so keep the ones not being saved now
  nlohmann::json json_data = nlohmann::json::object();

  if (std::ifstream in(filepath); in.is_open()) {
    try {
      nlohmann::json parsed = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
      if (parsed.is_object()) json_data = std::move(parsed);
    } catch (const std::exception& e) {
      // File is not even readable (e.g. it is a directory), so there is nothing to keep from it
      WARN("Cannot read settings file=", std::quoted(filepath.string()), ", error=", e.what());
    }
  }

  auto section = [&json_data](const char* name) -> nlohmann::json& {
    if (!json_data[name].is_object()) json_data[name] = nlohmann::json::object();
    return json_data[name];
  };

  if (settings.animation)
    section("visualizer")["animation"] = model::GetAnimationId(*settings.animation);
  if (settings.bar_width) section("visualizer")["bar_width"] = *settings.bar_width;
  if (settings.bar_style) section("visualizer")["bar_style"] = model::GetId(*settings.bar_style);
  if (settings.bar_spacing) section("visualizer")["bar_spacing"] = *settings.bar_spacing;
  if (settings.bar_colors) section("visualizer")["bar_colors"] = model::GetId(*settings.bar_colors);
  if (settings.peaks) section("visualizer")["peaks"] = *settings.peaks;
  if (settings.hue_cycle) section("visualizer")["hue_cycle"] = model::GetId(*settings.hue_cycle);
  if (settings.volume) section("player")["volume"] = *settings.volume;
  if (settings.device) section("player")["device"] = *settings.device;
  if (settings.repeat) section("player")["repeat"] = model::GetRepeatModeName(*settings.repeat);
  if (settings.shuffle) section("player")["shuffle"] = *settings.shuffle;
  if (settings.equalizer_preset) section("equalizer")["preset"] = *settings.equalizer_preset;
  if (settings.equalizer_custom) section("equalizer")["custom"] = *settings.equalizer_custom;
  if (settings.theme) section("interface")["theme"] = *settings.theme;
  if (settings.cookies_from_browser)
    section("stream")["cookies_from_browser"] = *settings.cookies_from_browser;

  std::error_code error;

  // Check that parent directory exists
  if (!CreateDirectory(filepath.parent_path(), error)) {
    ERROR("Cannot create parent directory for settings file, error=", error);
    return false;
  }

  std::ofstream out(filepath.string());

  if (!out.is_open()) {
    ERROR("Cannot open file for writing settings");
    return false;
  }

  // Pretty print JSON data with indentation of 2 spaces
  out << std::setw(2) << json_data;

  if (out.fail()) {
    ERROR("Failed to write JSON");
    return false;
  }

  return true;
}

/* ********************************************************************************************** */

bool FileHandler::ParseStats(model::ListeningStats& stats) {
  std::string file_path{GetStatsPath()};

  if (!std::filesystem::exists(file_path)) return false;

  nlohmann::json parsed;

  try {
    std::ifstream json(file_path);
    parsed = nlohmann::json::parse(json);
  } catch (const std::exception& e) {
    // Besides invalid JSON, file may not even be readable (e.g. it is a directory)
    ERROR("Cannot parse stats file=", std::quoted(file_path), ", error=", e.what());
    internal::BackupFile(file_path);
    return false;
  }

  if (!parsed.is_object()) {
    ERROR("Stats file does not contain an object, file=", std::quoted(file_path));
    internal::BackupFile(file_path);
    return false;
  }

  model::ListeningStats tmp;
  internal::ReadUnsigned(parsed, "since", tmp.since);
  internal::ReadUnsigned(parsed, "listened_ms", tmp.listened_ms);
  internal::ReadUnsigned(parsed, "local_ms", tmp.local_ms);
  internal::ReadUnsigned(parsed, "stream_ms", tmp.stream_ms);
  internal::ReadUnsigned(parsed, "songs_played", tmp.songs_played);
  internal::ReadUnsigned(parsed, "songs_finished", tmp.songs_finished);
  internal::ReadUnsigned(parsed, "songs_skipped", tmp.songs_skipped);
  internal::ReadUnsigned(parsed, "stream_bytes", tmp.stream_bytes);

  if (auto songs = parsed.find("songs"); songs != parsed.end() && songs->is_array()) {
    for (const auto& song : *songs) {
      if (!song.is_object() || !song.contains("key") || !song["key"].is_string()) continue;

      model::ListeningStats::Song entry;
      if (auto it = song.find("artist"); it != song.end() && it->is_string()) entry.artist = *it;
      if (auto it = song.find("title"); it != song.end() && it->is_string()) entry.title = *it;
      internal::ReadUnsigned(song, "listened_ms", entry.listened_ms);
      internal::ReadUnsigned(song, "plays", entry.plays);

      if (tmp.songs.size() < model::ListeningStats::kMaxSongs) {
        tmp.songs[song["key"].get<std::string>()] = std::move(entry);
      }
    }
  }

  // Hours are only filled when there is one list for each weekday with a value for each hour
  auto is_list = [](const nlohmann::json& value, std::size_t size) {
    return value.is_array() && value.size() == size;
  };

  if (auto hours = parsed.find("hours");
      hours != parsed.end() && is_list(*hours, model::ListeningStats::kWeekdays) &&
      std::all_of(hours->begin(), hours->end(), [&is_list](const nlohmann::json& day) {
        return is_list(day, model::ListeningStats::kDayHours);
      })) {
    for (std::size_t day = 0; day < model::ListeningStats::kWeekdays; day++) {
      for (std::size_t hour = 0; hour < model::ListeningStats::kDayHours; hour++) {
        if (const auto& value = (*hours)[day][hour]; value.is_number_unsigned()) {
          tmp.hours[day][hour] = value.get<uint64_t>();
        }
      }
    }
  }

  if (auto streak = parsed.find("streak"); streak != parsed.end() && streak->is_object()) {
    internal::ReadUnsigned(*streak, "day", tmp.streak.day);
    internal::ReadUnsigned(*streak, "current", tmp.streak.current);
    internal::ReadUnsigned(*streak, "longest", tmp.streak.longest);
  }

  LOG("Parsed listening stats with songs=", tmp.songs.size());
  stats = std::move(tmp);
  return true;
}

/* ********************************************************************************************** */

bool FileHandler::SaveStats(const model::ListeningStats& stats) {
  nlohmann::json json_songs = nlohmann::json::array();

  for (const auto& [key, song] : stats.songs) {
    json_songs.push_back({{"key", key},
                          {"artist", song.artist},
                          {"title", song.title},
                          {"listened_ms", song.listened_ms},
                          {"plays", song.plays}});
  }

  nlohmann::json json_data{{"since", stats.since},
                           {"listened_ms", stats.listened_ms},
                           {"local_ms", stats.local_ms},
                           {"stream_ms", stats.stream_ms},
                           {"songs_played", stats.songs_played},
                           {"songs_finished", stats.songs_finished},
                           {"songs_skipped", stats.songs_skipped},
                           {"stream_bytes", stats.stream_bytes},
                           {"songs", json_songs},
                           {"hours", stats.hours},
                           {"streak",
                            {{"day", stats.streak.day},
                             {"current", stats.streak.current},
                             {"longest", stats.streak.longest}}}};

  std::filesystem::path filepath{GetStatsPath()};
  std::error_code error;

  // Check that parent directory exists
  if (!CreateDirectory(filepath.parent_path(), error)) {
    ERROR("Cannot create parent directory for stats file, error=", error);
    return false;
  }

  // Written to another file first, so the one saved before is not lost if writing fails midway
  std::filesystem::path temporary{filepath.string() + ".tmp"};

  {
    std::ofstream out(temporary.string());

    if (!out.is_open()) {
      ERROR("Cannot open file for writing stats");
      return false;
    }

    out << std::setw(2) << json_data;

    if (out.fail()) {
      ERROR("Failed to write JSON");
      return false;
    }
  }

  if (std::filesystem::rename(temporary, filepath, error); error) {
    ERROR("Cannot replace stats file, error=", error.message());
    return false;
  }

  return true;
}

/* ********************************************************************************************** */

bool FileHandler::CreateDirectory(std::string const& path, std::error_code& error) {
  error.clear();

  if (std::filesystem::create_directories(path, error)) {
    return true;
  }

  // Folder already exists
  if (std::filesystem::exists(path)) {
    error.clear();
    return true;
  }

  return false;
}

}  // namespace util
