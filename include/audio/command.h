/**
 * \file
 * \brief Structure for an audio player command
 */

#ifndef INCLUDE_AUDIO_COMMAND_H_
#define INCLUDE_AUDIO_COMMAND_H_

#include <iostream>
#include <variant>

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
    Exit = 8006,
  };

  //! Overloaded operators
  bool operator==(const Command& other) const { return id == other.id; }
  bool operator!=(const Command& other) const { return !operator==(other); }

  bool operator==(const Identifier& other) const { return id == other; }
  bool operator!=(const Identifier& other) const { return !operator==(other); }

  // Fancy way to access command identifier
  Identifier& operator&() { return id; }

  //! Output command to ostream
  friend std::ostream& operator<<(std::ostream& out, Command& cmd) {
    switch (&cmd) {
      case Command::Identifier::None:
        out << " None ";
        break;
      case Command::Identifier::Play:
        out << " Play ";
        break;
      case Command::Identifier::PauseOrResume:
        out << " PauseOrResume ";
        break;
      case Command::Identifier::Stop:
        out << " Stop ";
        break;
      case Command::Identifier::SeekForward:
        out << " SeekForward ";
        break;
      case Command::Identifier::SeekBackward:
        out << " SeekBackward ";
        break;
      case Command::Identifier::Exit:
        out << " Exit ";
        break;
    }

    return out;
  }

 public:
  //! Possible commands to be handled by audio player
  static Command None();
  static Command Play();
  static Command PauseOrResume();
  static Command Stop();
  static Command SeekForward(int offset);
  static Command SeekBackward(int offset);
  static Command Exit();

  //! Possible types for content
  using Content = std::variant<int>;

  //! Generic getter for event content
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
  Identifier id;    //!< Unique type identifier for Event
  Content content;  //!< Wrapper for content
};

}  // namespace audio

#endif  // INCLUDE_AUDIO_COMMAND_H_
