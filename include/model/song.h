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

  //! Audio state
  enum class MediaState {
    Empty = 2001,
    Play = 2002,
    Pause = 2003,
    Stop = 2004,
  };

  struct CurrentInformation {
    MediaState state;   //!< Current song state
    uint32_t position;  //!< Current position (in seconds) of the audio

    //! Overloaded operators
    bool operator==(const CurrentInformation& other) const;
    bool operator!=(const CurrentInformation& other) const;
  };

  CurrentInformation curr_info;  //!< Current state of song
};

/**
 * @brief Util method to pretty print Song structure
 * @param arg Song struct
 * @return std::string Formatted string with properties from Song
 */
std::string to_string(const Song& arg);

/**
 * @brief Util method to pretty print time from Song structure
 * @param arg Time (in seconds)
 * @return std::string Formatted string with converted time from Song
 */
std::string time_to_string(const uint32_t& arg);

}  // namespace model
#endif  // INCLUDE_MODEL_SONG_H_
