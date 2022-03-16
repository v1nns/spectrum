#include "ui/base/terminal.h"

#include <stdlib.h>
#include <unistd.h>

#include <cctype>
#include <functional>
#include <iostream>

#include "error_code.h"
#include "ui/colors.h"

namespace interface {

constexpr int kDelayLoop = 5000;

/* ********************************************************************************************** */

int Terminal::Init() {
  // Create and initialize window
  auto* window = initscr();
  if (window == nullptr) {
    // TODO: fix this or use it everywhere
    fputs("Could not initialize screen.\n", stderr);
    return ERR_GENERIC;
  }

  // Check color availability
  if (!has_colors() || !can_change_color()) {
    endwin();
    refresh();

    fputs("Do not have availability to change colors.\n", stderr);
    return ERR_GENERIC;
  }

  InitializeColors();

  // Hide cursor, disable echo and remove timeout to execute a non-blocking polling
  curs_set(FALSE);
  timeout(0);
  keypad(stdscr, TRUE);
  noecho();
  raw();

  // Get terminal dimension
  max_size_ = GetScreenSize();

  return ERR_OK;
}

/* ********************************************************************************************** */

int Terminal::Destroy() {
  // Destroy windows from all blocks
  for (auto& block : blocks_) {
    block->Destroy();
  }

  // Delete terminal window
  endwin();
  refresh();

  return ERR_OK;
}

/* ********************************************************************************************** */

void Terminal::InitializeColors() {
  // Default ncurses initialization
  start_color();
  use_default_colors();

  // Create custom colors
  init_pair(kColorTextGreen, COLOR_GREEN, -1);
}

/* ********************************************************************************************** */

void Terminal::OnResize() {
  // Here it is where the magic happens for window resize:
  // ncurses will re-initialize itself with the new terminal dimensions.
  endwin();
  refresh();
  clear();

  // Get new terminal dimension
  max_size_ = GetScreenSize();

  // Every block must resize its own internal size
  for (auto& block : blocks_) {
    block->ResizeWindow(max_size_);
  }

  // Force a window refresh
  wnoutrefresh(stdscr);
}

/* ********************************************************************************************** */

void Terminal::OnPolling() {
  int key = getch();

  // Global commands
  if (has_focus_) {
    HandleInput(key);
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

  doupdate();  // Read: https://linux.die.net/man/3/doupdate
}

/* ********************************************************************************************** */

void Terminal::HandleInput(int key) {
  switch (std::tolower(key)) {
    case 'q':
      exit_ = true;
      break;
  }
}

/* ********************************************************************************************** */

void Terminal::AppendBlock(std::unique_ptr<Block>& b) {
  // TODO: add size control here with assert
  b->Init(max_size_);
  b->RegisterCallback(std::bind(&Terminal::SetFocus, this, std::placeholders::_1));
  blocks_.push_back(std::move(b));
}

/* ********************************************************************************************** */

void Terminal::SetFocus(bool focused) { has_focus_ = !focused; }

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

  usleep(kDelayLoop);
  return !exit_;
}

/* ********************************************************************************************** */

screen_size_t Terminal::GetScreenSize() {
  screen_size_t size{0, 0};
  getmaxyx(stdscr, size.row, size.column);

  return size;
}

}  // namespace interface