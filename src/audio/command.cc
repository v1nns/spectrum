#include "audio/command.h"

namespace audio {

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