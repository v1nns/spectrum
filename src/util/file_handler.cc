#include "util/file_handler.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <set>
#include <string_view>
#include <system_error>

#include "nlohmann/json.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "util/url.h"

namespace util {

namespace internal {

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
        .artist = song.contains("artist") ? util::filter_ascii(song["artist"]) : "",
        .title = song.contains("title") ? util::filter_ascii(song["title"]) : "",
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

std::string FileHandler::GetPlaylistsPath() const {
  return std::string{GetHome() + "/.cache/spectrum/playlists.json"};
}

/* ********************************************************************************************** */

std::string FileHandler::GetSettingsPath() const {
  return std::string{GetHome() + "/.cache/spectrum/settings.json"};
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
  } catch (const nlohmann::json::exception& e) {
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
      ERROR("Skipping playlist with missing or invalid name/songs, playlist=", playlist.dump());
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
        ERROR("Skipping invalid song=", song.dump(), " from playlist=", std::quoted(entry.name),
              ", error=", e.what());
        skipped = true;
      }
    }

    // Append playlist
    tmp.push_back(entry);
  }

  if (skipped) internal::BackupFile(file_path);

  LOG("Parsed ", tmp.size(), " playlists");
  playlists = std::move(tmp);
  return true;
}

/* ********************************************************************************************** */

bool FileHandler::SavePlaylists(const model::Playlists& playlists) {
  // Start by parsing c++ model structure into JSON structure
  nlohmann::json json_playlists;

  for (const auto& playlist : playlists) {
    nlohmann::json json_playlist, json_songs;

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
  } catch (const nlohmann::json::exception& e) {
    ERROR("Cannot parse settings file=", std::quoted(file_path), ", error=", e.what());
    internal::BackupFile(file_path);
    return false;
  }

  auto visualizer = parsed.is_object() ? parsed.find("visualizer") : parsed.end();
  if (visualizer == parsed.end() || !visualizer->is_object()) {
    ERROR("Settings file does not contain visualizer settings, file=", std::quoted(file_path));
    return false;
  }

  // Animation is saved by its identifier, so check it is a known one
  if (auto animation = visualizer->find("animation");
      animation != visualizer->end() && animation->is_number_integer()) {
    if (int value = animation->get<int>();
        value >= model::BarAnimation::HorizontalMirror && value < model::BarAnimation::LAST) {
      settings.animation = static_cast<model::BarAnimation>(value);
    }
  }

  if (auto bar_width = visualizer->find("bar_width");
      bar_width != visualizer->end() && bar_width->is_number_integer()) {
    settings.bar_width = bar_width->get<int>();
  }

  LOG("Parsed settings from file=", std::quoted(file_path));
  return true;
}

/* ********************************************************************************************** */

bool FileHandler::SaveSettings(const model::Settings& settings) {
  nlohmann::json visualizer = nlohmann::json::object();
  if (settings.animation) visualizer["animation"] = static_cast<int>(*settings.animation);
  if (settings.bar_width) visualizer["bar_width"] = *settings.bar_width;

  nlohmann::json json_data;
  json_data["visualizer"] = visualizer;

  std::filesystem::path filepath{GetSettingsPath()};
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
