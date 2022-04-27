/**
 * \file
 * \brief  Base class for a song
 */

#ifndef INCLUDE_MODEL_SONG_H_
#define INCLUDE_MODEL_SONG_H_

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

/**
 * @brief Interface class for a Song
 */
class Song {
 public:
  /**
   * @brief Construct a new Song object
   */
  Song() = default;

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
   * @brief Parse only the header metadata from a given sound file
   *
   * @param full_path Path where song is located
   * @return int Error code from operation
   */
  virtual int ParseHeaderInfo(const std::string& full_path) = 0;

  /**
   * @brief Parse raw data from a given sound file (this is only possible after parsing header info)
   * @return int Error code from operation
   */
  virtual int ParseData() = 0;

  /**
   * @brief Get the Formatted Stats from parsed sound file
   * @return std::vector<std::string> Text splitted in lines
   */
  virtual std::vector<std::string> GetFormattedStats() = 0;

  /* ******************************************************************************************** */
 protected:
  std::string filename_;  //!< Path to sound file
  std::ifstream file_;    //!< File-based streambuffer pointing to sound file
};

#endif  // INCLUDE_MODEL_SONG_H_