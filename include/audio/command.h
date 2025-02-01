/**
 * \file
 * \brief Structure for an audio player command
 */

#ifndef INCLUDE_AUDIO_COMMAND_H_
#define INCLUDE_AUDIO_COMMAND_H_

#include <iostream>
#include <variant>
#include <vector>

#include "model/audio_filter.h"
#include "model/song.h"
#include "model/volume.h"

namespace audio {

/**
 * @brief Interface for commands to be handled by audio player
 */
struct Command {
  //! Identifier for all existing events
  enum class Identifier {
    None = 8000,
    Play = 8001,
    PauseOrResume = 8002,
    Stop = 8003,
    SeekForward = 8004,
    SeekBackward = 8005,
    SetVolume = 8006,
    UpdateAudioFilters = 8007,
    Exit = 8008,
  };

  //! Overloaded operators
  friend bool operator==(const Command& lhs, const Command& rhs) { return lhs.id == rhs.id; }
  friend bool operator!=(const Command& lhs, const Command& rhs) { return lhs.id != rhs.id; }
  friend bool operator==(const Command& lhs, const Command::Identifier& rhs) {
    return lhs.id == rhs;
  }
  friend bool operator!=(const Command& lhs, const Command::Identifier& rhs) {
    return lhs.id != rhs;
  }

  //! Output command to ostream
  friend std::ostream& operator<<(std::ostream& out, const Command::Identifier& i);
  friend std::ostream& operator<<(std::ostream& out, const std::vector<Command::Identifier>& cmds);
  friend std::ostream& operator<<(std::ostream& out, const Command& cmd);
  friend std::ostream& operator<<(std::ostream& out, const std::vector<Command>& cmds);

  //! Possible commands to be handled by audio player
  static Command None();
  static Command Play(const model::Song& song);
  static Command PauseOrResume();
  static Command Stop();
  static Command SeekForward(int offset);
  static Command SeekBackward(int offset);
  static Command SetVolume(const model::Volume& value);
  static Command UpdateAudioFilters(const model::EqualizerPreset& filters);
  static Command Exit();

  //! Possible types for content
  using Content =
      std::variant<std::monostate, model::Song, int, model::Volume, model::EqualizerPreset>;

  //! Getter for command identifier
  Identifier GetId() const { return id; }

  //! Generic getter for command content
  template <typename T>
  T GetContent() const {
    if (std::holds_alternative<T>(content)) {
      return std::get<T>(content);
    }
    return T();
  }

  //! Variables
  // P.S. removed private keyword, otherwise wouldn't be possible to use C++ brace initialization
  Identifier id;    //!< Unique type identifier for Command
  Content content;  //!< Wrapper for content
};

}  // namespace audio

#endif  // INCLUDE_AUDIO_COMMAND_H_
