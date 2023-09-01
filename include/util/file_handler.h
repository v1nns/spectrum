/**
 * \file
 * \brief  Class for file handling (for operations like listing files in a dir, read/write, etc)
 */

#ifndef INCLUDE_UTIL_FILE_HANDLER_H_
#define INCLUDE_UTIL_FILE_HANDLER_H_

#include <filesystem>
#include <vector>

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
   * @brief List all files from the given directory path
   * @param dir_path Full path to directory
   * @param parsed_files[out] Existing files in the given directory path
   * @return true if directory was parsed succesfully, false otherwise
   */
  bool ListFiles(const std::filesystem::path& dir_path, Files& parsed_files);

  /* ******************************************************************************************** */
  //! Private implementation
 private:
  /* ******************************************************************************************** */
  //! Variables
};

}  // namespace util
#endif  // INCLUDE_UTIL_FILE_HANDLER_H_
