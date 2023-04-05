/**
 * \file
 * \brief Structure for an audio player command
 */

#ifndef INCLUDE_AUDIO_COMMAND_H_
#define INCLUDE_AUDIO_COMMAND_H_

#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "model/audio_filter.h"
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
  bool operator==(const Command& other) const { return id == other.id; }
  bool operator!=(const Command& other) const { return !operator==(other); }

  bool operator==(const Identifier& other) const { return id == other; }
  bool operator!=(const Identifier& other) const { return !operator==(other); }

  //! Output command to ostream
  friend std::ostream& operator<<(std::ostream& out, const Command& cmd);

  //! Possible commands to be handled by audio player
  static Command None();
  static Command Play(const std::string& filepath = "");
  static Command PauseOrResume();
  static Command Stop();
  static Command SeekForward(int offset);
  static Command SeekBackward(int offset);
  static Command SetVolume(const model::Volume& value);
  static Command UpdateAudioFilters(const model::EqualizerPreset& filters);
  static Command Exit();

  //! Possible types for content
  using Content =
      std::variant<std::monostate, std::string, int, model::Volume, model::EqualizerPreset>;

  //! Getter for command identifier
  Identifier GetId() const { return id; }

  //! Generic getter for command content
  template <typename T>
  T GetContent() const {
    if (std::holds_alternative<T>(content)) {
      return std::get<T>(content);
    } else {
      return T();
    }
  }

  //! Variables
  // P.S. removed private keyword, otherwise wouldn't be possible to use C++ brace initialization
  Identifier id;    //!< Unique type identifier for Command
  Content content;  //!< Wrapper for content
};

}  // namespace audio

#endif  // INCLUDE_AUDIO_COMMAND_H_
