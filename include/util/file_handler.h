/**
 * \file
 * \brief  Class for file handling (for operations like listing files in a dir, read/write, etc)
 */

#ifndef INCLUDE_UTIL_FILE_HANDLER_H_
#define INCLUDE_UTIL_FILE_HANDLER_H_

#include <filesystem>
#include <vector>

#include "model/playlist.h"

namespace util {

//! For better readability
using File = std::filesystem::path;  //!< Single file path
using Files = std::vector<File>;     //!< List of file paths

/**
 * @brief Class responsible to perform any file I/O operation
 */
class FileHandler {
 public:
  /**
   * @brief Construct a new FileHandler object
   */
  FileHandler() = default;

  /**
   * @brief Destroy the FileHandler object
   */
  ~FileHandler() = default;

  //! Remove these
  FileHandler(const FileHandler& other) = delete;             // copy constructor
  FileHandler(FileHandler&& other) = delete;                  // move constructor
  FileHandler& operator=(const FileHandler& other) = delete;  // copy assignment
  FileHandler& operator=(FileHandler&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Get full path to home directory
   * @return String containing directory path
   */
  std::string GetHome() const;

  /**
   * @brief Get full path to playlist JSON file
   * @return String containing filepath
   */
  std::string GetPlaylistsPath() const;

  /**
   * @brief List all files from the given directory path
   * @param dir_path Full path to directory
   * @param parsed_files[out] Existing files in the given directory path
   * @return true if directory was parsed succesfully, false otherwise
   */
  bool ListFiles(const std::filesystem::path& dir_path, Files& parsed_files);

  /**
   * @brief Parse playlists from JSON
   * @param playlists[out] Playlists object filled by data from JSON parsed
   * @return true if JSON was parsed succesfully, false otherwise
   */
  virtual bool ParsePlaylists(model::Playlists& playlists);

  /**
   * @brief Save playlists to JSON
   * @param playlists Playlists object already filled
   * @return true if JSON was saved succesfully, false otherwise
   */
  virtual bool SavePlaylists(const model::Playlists& playlists);
};

}  // namespace util
#endif  // INCLUDE_UTIL_FILE_HANDLER_H_
