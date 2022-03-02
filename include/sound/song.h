/**
 * \file
 * \brief  Base class for a song
 */

#ifndef INCLUDE_SONG_H_
#define INCLUDE_SONG_H_

#include <cstdio>
#include <string>
#include <vector>

/**
 * @brief Base class for a Song
 */
class Song {
 public:
  /**
   * @brief Construct a new Song object
   */
  Song() : filename_(), file_(nullptr), length_(0){};

  /**
   * @brief Destroy the Song object
   */
  virtual ~Song() = default;

  /* ******************************************************************************************** */
  // Remove these constructors/operators
  Song(const Song& other) = delete;             // copy constructor
  Song(Song&& other) = delete;                  // move constructor
  Song& operator=(const Song& other) = delete;  // copy assignment
  Song& operator=(Song&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  /**
   * @brief Parse a given sound file to get its info
   *
   * @param full_path Text containing path where file is located
   * @return int Error code from operation
   */
  virtual int ParseFromFile(const std::string& full_path) = 0;

  /**
   * @brief Get the Formatted Stats from parsed sound file
   *
   * @return std::vector<std::string> Text splitted in lines
   */
  virtual std::vector<std::string> GetFormattedStats() = 0;

  /* ******************************************************************************************** */
 protected:
  std::string filename_;  //!< Path to sound file
  FILE* file_;            //!< File descriptor to sound file
  long length_;           //!< File length in bytes
};

#endif  // INCLUDE_SONG_H_