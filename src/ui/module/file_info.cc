#include "ui/module/file_info.h"

namespace interface {

// TODO: remove this
#define SONG_PATH_FOR_DEV "/home/vinicius/projects/music-analyzer/africa-toto.wav"

/* ********************************************************************************************** */

FileInfo::FileInfo(point_t init, screen_size_t size)
    : Block(init, size, "File Information", FileInfo::InitialState::GetInstance()) {}

/* ********************************************************************************************** */

void FileInfo::InitialState::Draw(Block& block) {
  auto window = block.GetWindow();
  werase(window);

  // Box content
  mvwprintw(window, 1, 2, "Hello, press \"SPACE\" to start.");

  wrefresh(window);
};

/* ********************************************************************************************** */

void FileInfo::InitialState::HandleInput(Block& block, char key) {
  switch (key) {
    case ' ': {
      ChangeState(block, FileInfo::ShowInfoState::GetInstance());
      break;
    }
    default:
      break;
  }
}

/* ********************************************************************************************** */

void FileInfo::ShowInfoState::Draw(Block& block) {
  auto window = block.GetWindow();
  werase(window);

  // Box content
  WaveFormat song;
  song.ParseFromFile(SONG_PATH_FOR_DEV);
  auto stats = song.GetFormattedStats();
  int row = 1;
  for (const auto& line : stats) {
    mvwprintw(window, row, 2, line.c_str());
    ++row;
  }

  wrefresh(window);
};

}  // namespace interface
