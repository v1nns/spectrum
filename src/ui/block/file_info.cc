#include "ui/block/file_info.h"

namespace interface {

// TODO: remove this
#define SONG_PATH_FOR_DEV "/home/vinicius/projects/music-analyzer/africa-toto.wav"

/* ********************************************************************************************** */

FileInfo::FileInfo() {}

/* ********************************************************************************************** */

Element FileInfo::Render() { return Element(); }

/* ********************************************************************************************** */

bool OnEvent(Event event) { return false; }

}  // namespace interface
