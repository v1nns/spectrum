#include "util/file_handler.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <regex>
#include <set>

#include "nlohmann/json.hpp"
#include "util/logger.h"

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

//! Basic URL validation pattern
static bool IsYoutubeValid(const std::string& url) {
  std::regex valid_url(R"(^(https?://)?(www\.)?(?:youtube\.com|youtu\.be)/.*$)");
  return std::regex_match(url, valid_url);
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

  std::ifstream json(file_path);
  nlohmann::json parsed = nlohmann::json::parse(json);

  LOG("Found playlist file, start parsing it");
  if (!parsed.contains("playlists")) return false;

  model::Playlists tmp;

  // Parse all playlists
  for (auto& [_, playlist] : parsed["playlists"].items()) {
    if (!playlist.contains("name") || !playlist.contains("songs")) continue;

    model::Playlist entry{.name = playlist["name"], .songs = {}};

    // To avoid including same song multiple times...
    std::set<std::filesystem::path> filepaths;

    // Parse all songs from a single playlist
    for (auto& [_, song] : playlist["songs"].items()) {
      if (song.contains("path") && std::filesystem::exists(song["path"])) {
        // Insert only if filepath is not duplicated
        if (auto [it, inserted] = filepaths.emplace(song["path"]); inserted) {
          // Song from filepath
          entry.songs.emplace_back(model::Song{
              .filepath = song["path"],
          });
        }

      } else if (song.contains("url") && internal::IsYoutubeValid(song["url"])) {
        // Song from URL
        entry.songs.emplace_back(model::Song{
            .stream_info = model::StreamInfo{.base_url = song["url"]},
        });
      }
    }

    // Append playlist
    tmp.push_back(entry);
  }

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
      json_song["path"] = song.filepath.string();
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

  try {
    // Pretty print JSON data with indentation of 2 spaces
    out << std::setw(2) << json_data;
  } catch (const std::exception& e) {
    ERROR("Failed to write JSON, error=", e.what());
    return false;
  }

  // Close the file
  out.close();

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
