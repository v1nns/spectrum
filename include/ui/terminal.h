/**************************************************************************************************/
/**
 * \file
 * \brief  Base class for plotting in terminal
 */
/**************************************************************************************************/

#ifndef INCLUDE_TERMINAL_H_
#define INCLUDE_TERMINAL_H_

#include <curses.h>

#define MAIN_WIN_COLOR 1

class Terminal {
 public:
  // Constructor
  Terminal() : win_(), exit_(false), max_column_(0), max_row_(0){};

  // Destructor
  ~Terminal() = default;

  // Remove these operators
  Terminal(const Terminal& other) = delete;             // copy constructor
  Terminal(Terminal&& other) = delete;                  // move constructor
  Terminal& operator=(const Terminal& other) = delete;  // copy assignment
  Terminal& operator=(Terminal&& other) = delete;       // move assignment

  bool Init();
  bool Cleanup();

  bool Tick();

 private:
  WINDOW* win_;
  bool exit_;

  short max_column_, max_row_;
};

#endif  // INCLUDE_TERMINAL_H_