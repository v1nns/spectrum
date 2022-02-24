/**************************************************************************************************/
/**
 * \file
 * \brief  Base class for a song
 */
/**************************************************************************************************/

#ifndef INCLUDE_SONG_H_
#define INCLUDE_SONG_H_

#include <cstdio>
#include <string>

class Song {
 public:
  // Constructor
  Song() : filename_(), file_(nullptr), length_(0){};

  // Destructor
  ~Song() = default;

  // Remove these operators
  Song(const Song& other) = delete;             // copy constructor
  Song(Song&& other) = delete;                  // move constructor
  Song& operator=(const Song& other) = delete;  // copy assignment
  Song& operator=(Song&& other) = delete;       // move assignment

  virtual bool ParseFromFile(const std::string& full_path) = 0;
  virtual void PrintStats() = 0;

 protected:
  std::string filename_;
  FILE* file_;
  long length_;
};

#endif  // INCLUDE_SONG_H_