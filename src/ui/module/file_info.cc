#include "ui/module/file_info.h"

namespace interface {

#define SONG_PATH_FOR_DEV "/home/vinicius/projects/music-analyzer/africa-toto.wav"

void FileInfo::Draw(bool rescale) {
  if (first_draw_) {
    DrawBorder();

    // Box content
    mvwprintw(win_, 1, 2, "Hello, press \"SPACE\" to start.");
    wrefresh(win_);

    first_draw_ = false;
  }

  // TODO: rescale

  //   if (wtf) {
  //     auto stats = song_.GetFormattedStats();
  //     int row = 1;
  //     for (const auto& line : stats) {
  //       mvwprintw(win_, row, 2, line.c_str());
  //       ++row;
  //     }
  //     // refreshing the window
  //     wrefresh(win_);
  //   }
};

void FileInfo::HandleInput(char key) {
  switch (key) {
    case ' ': {
      //   song_.ParseFromFile(SONG_PATH_FOR_DEV);
      break;
    }
    default:
      break;
  }
}

}  // namespace interface
