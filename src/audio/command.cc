#include "audio/command.h"

namespace audio {

//! Command::Identifier pretty print
std::ostream& operator<<(std::ostream& out, const Command::Identifier& i) {
  switch (i) {
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

//! Command pretty print
std::ostream& operator<<(std::ostream& out, const Command& cmd) {
  out << cmd.id;
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
Command Command::Play() {
  return Command{
      .id = Identifier::Play,
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
Command Command::Exit() {
  return Command{
      .id = Identifier::Exit,
  };
}

}  // namespace audio