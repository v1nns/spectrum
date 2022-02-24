#include "ui/terminal.h"

#include <stdlib.h>

#include <iostream>

#include "error_code.h"

bool Terminal::Init() {
  // Create and initialize window
  if ((win_ = initscr()) == NULL) {
    fputs("Could not initialize screen.", stderr);
    return ERR_GENERIC;
  }

  // Check color availability
  if (start_color() == ERR || !has_colors() || !can_change_color()) {
    delwin(win_);
    endwin();
    refresh();

    fputs("Could not use colors.", stderr);
    return ERR_GENERIC;
  }

  // Initialize colors
  init_pair(MAIN_WIN_COLOR, COLOR_WHITE, COLOR_BLUE);
  wbkgd(win_, COLOR_PAIR(MAIN_WIN_COLOR));

  // Hide cursor and remove timeout to use a non-blocking "getch" while polling
  curs_set(0);
  timeout(0);

  return ERR_OK;
}

bool Terminal::Cleanup() {
  // Clean up and exit
  delwin(win_);
  endwin();
  refresh();

  return ERR_OK;
}

bool Terminal::Tick() {
  // Get max coordinates every time refresh, otherwise it won't rescale
  getmaxyx(stdscr, max_row_, max_column_);

  // Draw box near the edge and hide cursor
  box(win_, 0, 0);

  char input = getch();

  if (input != ERR) {
    erase();
    move(max_row_ / 2, max_column_ / 2);
    printw("%c", input);
  }

  wrefresh(win_);
  return true;
}