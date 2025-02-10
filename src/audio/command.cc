#include "audio/command.h"

namespace audio {

//! Command::Identifier pretty print
std::ostream& operator<<(std::ostream& out, const Command::Identifier& i) {
  switch (i) {
    case Command::Identifier::None:
      out << "None";
      break;
    case Command::Identifier::Play:
      out << "Play";
      break;
    case Command::Identifier::PauseOrResume:
      out << "PauseOrResume";
      break;
    case Command::Identifier::Stop:
      out << "Stop";
      break;
    case Command::Identifier::SeekForward:
      out << "SeekForward";
      break;
    case Command::Identifier::SeekBackward:
      out << "SeekBackward";
      break;
    case Command::Identifier::SetVolume:
      out << "SetVolume";
      break;
    case Command::Identifier::UpdateAudioFilters:
      out << "UpdateAudioFilter";
      break;
    case Command::Identifier::Exit:
      out << "Exit";
      break;
  }

  return out;
}

//! Command::Identifiers pretty print
std::ostream& operator<<(std::ostream& out, const std::vector<Command::Identifier>& cmds) {
  if (cmds.empty()) {
    out << "[]";
    return out;
  }

  out << "[";

  std::vector<Command::Identifier>::const_iterator i, j;
  for (i = cmds.begin(), j = --cmds.end(); i != j; ++i) {
    out << "\"" << *i << "\"" << ",";
  }

  out << "\"" << *j << "\"";
  out << "]";

  return out;
}

//! Command pretty print
std::ostream& operator<<(std::ostream& out, const Command& cmd) {
  out << cmd.id;
  return out;
}

//! Commands pretty print
std::ostream& operator<<(std::ostream& out, const std::vector<Command>& cmds) {
  if (cmds.empty()) {
    out << "[]";
    return out;
  }

  out << "[";

  std::vector<Command>::const_iterator i, j;
  for (i = cmds.begin(), j = --cmds.end(); i != j; ++i) {
    out << "\"" << i->id << "\"" << ",";
  }

  out << "\"" << j->id << "\"";
  out << "]";

  return out;
}

/* ********************************************************************************************** */

// Static
Command Command::None() {
  return Command{
      .id = Identifier::None,
  };
}

/* ********************************************************************************************** */

// Static
Command Command::Play(const model::Song& song) {
  return Command{
      .id = Identifier::Play,
      .content = song,
  };
}

/* ********************************************************************************************** */

// Static
Command Command::PauseOrResume() {
  return Command{
      .id = Identifier::PauseOrResume,
  };
}

/* ********************************************************************************************** */

// Static
Command Command::Stop() {
  return Command{
      .id = Identifier::Stop,
  };
}

/* ********************************************************************************************** */

// Static
Command Command::SeekForward(int offset) {
  return Command{
      .id = Identifier::SeekForward,
      .content = offset,
  };
}

/* ********************************************************************************************** */

// Static
Command Command::SeekBackward(int offset) {
  return Command{
      .id = Identifier::SeekBackward,
      .content = offset,
  };
}

/* ********************************************************************************************** */

// Static
Command Command::SetVolume(const model::Volume& value) {
  return Command{
      .id = Identifier::SetVolume,
      .content = value,
  };
}

/* ********************************************************************************************** */

// Static
Command Command::UpdateAudioFilters(const model::EqualizerPreset& filters) {
  return Command{
      .id = Identifier::UpdateAudioFilters,
      .content = filters,
  };
}

/* ********************************************************************************************** */

// Static
Command Command::Exit() {
  return Command{
      .id = Identifier::Exit,
  };
}

}  // namespace audio
