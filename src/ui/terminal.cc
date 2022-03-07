#include "ui/base/terminal.h"

#include <stdlib.h>
#include <unistd.h>

#include <iostream>

#include "error_code.h"
namespace interface {

#define MAIN_WIN_COLOR 1
#define DELAY_LOOP 2000

/* ********************************************************************************************** */

int Terminal::Init() {
  // Create and initialize window
  win_ = initscr();
  if (win_ == nullptr) {
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

  // Hide cursor, disable echo and remove timeout to execute a non-blocking polling
  curs_set(FALSE);
  noecho();
  timeout(0);

  // Get terminal dimension
  max_size_ = GetCurrentScreenSize();

  return ERR_OK;
}

/* ********************************************************************************************** */

int Terminal::Destroy() {
  // Destroy windows from all blocks
  for (auto& block : blocks_) {
    block->Destroy();
  }

  // Delete terminal window
  delwin(win_);
  endwin();
  refresh();

  return ERR_OK;
}

/* ********************************************************************************************** */

void Terminal::OnResize() {
  // Here it is where the magic happens for window resize:
  // ncurses will re-initialize itself with the new terminal dimensions.
  endwin();
  refresh();
  clear();

  // Get new terminal dimension
  max_size_ = GetCurrentScreenSize();

  // Every block must resize its own internal size
  for (auto& block : blocks_) {
    block->ResizeWindow(max_size_);
  }

  // Force a window refresh
  wrefresh(win_);
}

/* ********************************************************************************************** */

void Terminal::OnPolling() {
  char key = getch();

  // Global commands
  if (key == 'q' || key == 'Q') {
    exit_ = true;
  }

  // Send key event to all blocks
  if (key != ERR) {
    for (auto& block : blocks_) {
      block->HandleInput(key);
    }
  }
}

/* ********************************************************************************************** */

void Terminal::OnDraw() {
  for (auto& block : blocks_) {
    block->Draw();
  }
}

/* ********************************************************************************************** */

void Terminal::AppendBlock(std::unique_ptr<Block>& b) {
  // TODO: add size control here with assert
  b->Init(max_size_);
  blocks_.push_back(std::move(b));
}

/* ********************************************************************************************** */

bool Terminal::Tick(volatile bool& resize) {
  if (resize) {
    OnResize();

    // Reset global flag
    resize = false;
  } else {
    OnPolling();
  }

  OnDraw();

  usleep(DELAY_LOOP);
  return !exit_;
}

/* ********************************************************************************************** */

screen_size_t Terminal::GetCurrentScreenSize() {
  screen_size_t size{0, 0};
  getmaxyx(stdscr, size.row, size.column);

  return size;
}

}  // namespace interface