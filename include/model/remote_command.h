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

namespace model {

/**
 * @brief Media command to control a running instance (same actions available by keyboard)
 */
enum class RemoteCommand : std::uint8_t {
  PlayOrPause,     //!< Play selected song, or pause/resume current one
  Play,            //!< Play selected song, or resume current one (nothing changes if playing)
  Pause,           //!< Pause current song (nothing changes if not playing)
  Stop,            //!< Stop current song
  SkipToPrevious,  //!< Skip to previous song from queue
  SkipToNext,      //!< Skip to next song from queue
  VolumeUp,        //!< Increase volume
  VolumeDown,      //!< Decrease volume
  Mute,            //!< Toggle volume mute
  SeekForward,     //!< Seek forward in current song
  SeekBackward,    //!< Seek backward in current song
  ToggleRepeat,    //!< Change repeat mode (off, all, one)
  ToggleShuffle,   //!< Toggle shuffle
};

//! All remote commands with the name used in command-line
inline constexpr std::array<std::pair<RemoteCommand, std::string_view>, 13> kRemoteCommands{{
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

}  // namespace model
#endif  // INCLUDE_MODEL_REMOTE_COMMAND_H_
