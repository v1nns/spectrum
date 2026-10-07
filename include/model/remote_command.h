/**
 * \file
 * \brief  Command sent from command-line to a running instance of this application
 */

#ifndef INCLUDE_MODEL_REMOTE_COMMAND_H_
#define INCLUDE_MODEL_REMOTE_COMMAND_H_

#include <array>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "model/repeat_mode.h"

namespace model {

/**
 * @brief Media command to control a running instance (same actions available by keyboard)
 */
enum class RemoteCommand : std::uint8_t {
  PlayOrPause,     //!< Play selected song, or pause/resume current one
  Play,            //!< Play selected song, or resume current one (nothing changes if playing).
                   //!< With a value: play that file, directory, URL or playlist
  Pause,           //!< Pause current song (nothing changes if not playing)
  Stop,            //!< Stop current song
  SkipToPrevious,  //!< Skip to previous song from queue
  SkipToNext,      //!< Skip to next song from queue
  VolumeUp,        //!< Increase volume
  VolumeDown,      //!< Decrease volume
  Mute,            //!< Toggle volume mute
  SeekForward,     //!< Seek forward in current song
  SeekBackward,    //!< Seek backward in current song
  ToggleRepeat,    //!< Change repeat mode (off, all, one), or set it to the given value
  ToggleShuffle,   //!< Toggle shuffle, or set it to the given value
  SetVolume,       //!< Set volume to the given value (or change it by that much)
  Seek,            //!< Seek to the given position in current song (or change it by that much)
  Quit,            //!< Exit from application
};

//! All remote commands with the name used in command-line
inline constexpr std::array<std::pair<RemoteCommand, std::string_view>, 16> kRemoteCommands{{
    {RemoteCommand::PlayOrPause, "play-pause"},
    {RemoteCommand::Play, "play"},
    {RemoteCommand::Pause, "pause"},
    {RemoteCommand::Stop, "stop"},
    {RemoteCommand::SkipToPrevious, "previous"},
    {RemoteCommand::SkipToNext, "next"},
    {RemoteCommand::VolumeUp, "volume-up"},
    {RemoteCommand::VolumeDown, "volume-down"},
    {RemoteCommand::Mute, "mute"},
    {RemoteCommand::SeekForward, "seek-forward"},
    {RemoteCommand::SeekBackward, "seek-backward"},
    {RemoteCommand::ToggleRepeat, "repeat"},
    {RemoteCommand::ToggleShuffle, "shuffle"},
    {RemoteCommand::SetVolume, "volume"},
    {RemoteCommand::Seek, "seek"},
    {RemoteCommand::Quit, "quit"},
}};

//! Name of the request to get player status (it is not a command, as nothing changes on player)
inline constexpr std::string_view kRemoteStatusQuery{"status"};

//! Name of the request to keep receiving player status, every time it changes
inline constexpr std::string_view kRemoteSubscribeQuery{"subscribe"};

//! Get remote command name
inline std::string_view GetRemoteCommandName(RemoteCommand command) {
  for (const auto& [id, name] : kRemoteCommands) {
    if (id == command) return name;
  }

  return "unknown";
}

//! Get remote command from its name (or nothing, if there is no command with this name)
inline std::optional<RemoteCommand> ParseRemoteCommand(std::string_view name) {
  for (const auto& [id, command_name] : kRemoteCommands) {
    if (command_name == name) return id;
  }

  return std::nullopt;
}

//! Get names from all remote commands and the status queries (separated by comma)
inline std::string GetRemoteCommandNames() {
  std::string names;

  for (const auto& [id, name] : kRemoteCommands) {
    names += name;
    names += ", ";
  }

  names += kRemoteStatusQuery;
  names += ", ";
  names += kRemoteSubscribeQuery;

  return names;
}

//! Output remote command to ostream
inline std::ostream& operator<<(std::ostream& out, RemoteCommand command) {
  return out << GetRemoteCommandName(command);
}

/**
 * @brief Number given to a remote command, either as the new value or as a change on current one
 */
struct RemoteNumber {
  int value;      //!< Number (negative only when relative)
  bool relative;  //!< Number is added to the current value, instead of replacing it

  //! Overloaded operators
  friend bool operator==(const RemoteNumber& lhs, const RemoteNumber& rhs);
  friend std::ostream& operator<<(std::ostream& out, const RemoteNumber& number);
};

/**
 * @brief Remote command with the value given to it (most commands do not have one)
 */
struct RemoteRequest {
  //! Nothing, a number (volume in percentage, or position in seconds), repeat mode, shuffle state
  //! or text (what to play)
  using Value = std::variant<std::monostate, RemoteNumber, RepeatMode, bool, std::string>;

  RemoteCommand command = RemoteCommand::PlayOrPause;  //!< Command to execute
  Value value;                                         //!< Value for command

  //! Default constructor
  RemoteRequest() = default;

  //! A command is also a request (not explicit, so it may be used wherever a request is expected)
  RemoteRequest(RemoteCommand id, Value content = {})  // NOLINT
      : command{id}, value{std::move(content)} {}

  //! Overloaded operators
  friend bool operator==(const RemoteRequest& lhs, const RemoteRequest& rhs);
  friend bool operator!=(const RemoteRequest& lhs, const RemoteRequest& rhs);
  friend std::ostream& operator<<(std::ostream& out, const RemoteRequest& request);
};

/**
 * @brief Get remote request from a line of text: command name, optionally followed by its value
 * (e.g. "next", "volume 50", "volume +5", "seek 1:30", "repeat all", "shuffle on", "play <path>")
 * @param text Line of text
 * @param error Reason why it is not a valid request (filled only when nothing is returned)
 * @return Remote request (or nothing, if command does not exist or does not accept this value)
 */
std::optional<RemoteRequest> ParseRemoteRequest(std::string_view text, std::string& error);

}  // namespace model
#endif  // INCLUDE_MODEL_REMOTE_COMMAND_H_
