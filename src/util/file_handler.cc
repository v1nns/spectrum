#include "util/file_handler.h"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <exception>
#include <fstream>
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

}  // namespace internal

/* ********************************************************************************************** */

std::string FileHandler::GetHome() const {
  const char* home_dir;

  if ((home_dir = getenv("HOME")) == nullptr) {
    home_dir = getpwuid(getuid())->pw_dir;
  }

  return std::string{home_dir};
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
  std::string file_path{GetHome() + "/.cache/spectrum/playlists.json"};

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
    std::set<std::filesystem::path> filepaths;

    // Parse all songs from a single playlist
    for (auto& [_, song] : playlist["songs"].items()) {
      if (!song.contains("path") || !std::filesystem::exists(song["path"])) continue;

      // Insert only if filepath is not duplicated
      if (auto [it, inserted] = filepaths.emplace(song["path"]); inserted) {
        entry.songs.emplace_back(model::Song{
            .filepath = song["path"],
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
  std::string file_path{"/tmp/dummy.json"};
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

  // Open the file in write mode
  std::ofstream out(file_path);

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

}  // namespace util
