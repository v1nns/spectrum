/**
 * \file
 * \brief  Base class for a song
 */

#ifndef INCLUDE_MODEL_SONG_H_
#define INCLUDE_MODEL_SONG_H_

#include <cstdio>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>

namespace model {

/**
 * @brief Detailed audio metadata information from song
 */
struct Song {
  std::string filepath;  //!< Full path to file
  std::string artist;    //!< Song artist name
  std::string title;     //!< Song title name

  uint16_t num_channels;  //!< Number of channels (1=Mono 2=Stereo)
  uint32_t sample_rate;   //!< Number of samples (of signal amplitude or “sound”) per second
  uint32_t bit_rate;      //!< Bits per second
  uint32_t bit_depth;     //!< Number of bits per sample
  uint32_t duration;      //!< Audio duration (in seconds)
};

/* ------------------------------------ Overloaded Operators ------------------------------------ */

std::ostream& operator<<(std::ostream& os, const Song& entry);
std::istream& operator>>(std::istream& is, Song& entry);

std::string to_string(const Song& arg);

}  // namespace model
#endif  // INCLUDE_MODEL_SONG_H_