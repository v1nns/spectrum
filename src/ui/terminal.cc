#include "ui/terminal.h"

#include <stdlib.h>
#include <unistd.h>

#include <iostream>

#include "error_code.h"
namespace interface {

#define MAIN_WIN_COLOR 1
#define DELAY_LOOP 2000

int Terminal::Init() {
  // Create and initialize window
  win_ = initscr();
  if (win_ == NULL) {
    // TODO: fix this or use it everywhere
    fputs("Could not initialize screen.\n", stderr);
    return ERR_GENERIC;
  }

  // Check color availability
  if (start_color() == ERR || !has_colors() || !can_change_color()) {
    delwin(win_);
    endwin();
    refresh();

    fputs("Could not use colors.\n", stderr);
    return ERR_GENERIC;
  }

  // Initialize colors
  init_pair(MAIN_WIN_COLOR, COLOR_BLUE, COLOR_BLACK);
  wbkgd(win_, COLOR_PAIR(MAIN_WIN_COLOR));

  // Hide cursor, disable echo and remove timeout to use a non-blocking "getch" while polling
  curs_set(FALSE);
  noecho();
  timeout(0);

  // Get max screen size
  getmaxyx(stdscr, max_row_, max_column_);

  return ERR_OK;
}

int Terminal::Destroy() {
  // Destroy stuff from every block
  for (auto& block : blocks_) {
    block->Destroy();
  }

  // Delete terminal window
  delwin(win_);
  endwin();
  refresh();

  return ERR_OK;
}

void Terminal::AppendBlock(std::unique_ptr<Block>& b) {
  // TODO: add size control here with assert
  b->Init(max_row_, max_column_);
  blocks_.push_back(std::move(b));
}

void Terminal::PollingInput() {
  char input = getch();

  if (input == 'q' || input == 'Q') {
    exit_ = true;
  }

  if (input != ERR) {
    for (auto& block : blocks_) {
      block->HandleInput(input);
    }
  }
}

bool Terminal::Tick() {
  PollingInput();

  Draw();

  usleep(DELAY_LOOP);
  return !exit_;
}

bool Terminal::IsDimensionUpdated() {
  bool updated = false;

  // Update coordinates every time, otherwise it won't rescale
  short row{0}, column{0};
  getmaxyx(stdscr, row, column);

  if (row != max_row_ || column != max_column_) {
    max_row_ = row;
    max_column_ = column;
    updated = true;
  }

  return updated;
}

void Terminal::Draw() {
  bool rescale = IsDimensionUpdated();

  for (auto& block : blocks_) {
    block->Draw(rescale);
  }
}

// void Terminal::WriteText(std::vector<std::string> lines) {
//   uint8_t row = 1;
//   for (const auto& line : lines) {
//     move(row, 1);
//     printw(line.c_str());
//     ++row;
//   }
// }

}  // namespace interface