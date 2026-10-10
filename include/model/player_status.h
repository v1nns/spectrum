/**
 * \file
 * \brief  Snapshot of player state, sent as reply to a status query from command-line
 */

#ifndef INCLUDE_MODEL_PLAYER_STATUS_H_
#define INCLUDE_MODEL_PLAYER_STATUS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "model/audio_output.h"
#include "model/repeat_mode.h"
#include "model/song.h"
#include "model/volume.h"

namespace model {

/**
 * @brief Small summary of what is known about the player at some point in time
 */
struct PlayerStatus {
  Song::MediaState state = Song::MediaState::Empty;  //!< Current song state

  std::string artist;  //!< Song artist name (empty when unknown, or without a song)
  std::string title;   //!< Song title name (or filename/URL when unknown, empty without a song)

  uint32_t position = 0;  //!< Current position (in seconds) of the audio
  uint32_t duration = 0;  //!< Audio duration (in seconds)

  //! Output device and format of audio samples sent to it (nothing without a song)
  std::optional<AudioOutput> output;

  Volume volume;                        //!< General sound volume
  RepeatMode repeat = RepeatMode::Off;  //!< Repeat mode for songs from queue
  bool shuffle = false;                 //!< Shuffle songs from queue
};

/**
 * @brief Convert player status to a JSON object, written in a single line
 * @param status Player status
 * @return std::string JSON object with state ("playing", "paused" or "stopped"), artist, title,
 * position and duration (in seconds), volume (from 0 to 100), muted, repeat ("off", "all" or "one")
 * and shuffle
 */
std::string to_json(const PlayerStatus& status);

/**
 * @brief Replace each field name between braces (e.g. "{artist} - {title}") by its value from
 * player status. Position and duration are written as time, and flags as "on"/"off"
 * @param json Player status as a JSON object (the one created by to_json)
 * @param format Text with fields to replace (anything else is kept, including unknown fields). If
 * empty, the JSON object itself is returned
 * @return std::string Formatted text (or nothing, if it is not a JSON object)
 */
std::optional<std::string> format_status(const std::string& json,
                                         const std::optional<std::string>& format);

}  // namespace model
#endif  // INCLUDE_MODEL_PLAYER_STATUS_H_
