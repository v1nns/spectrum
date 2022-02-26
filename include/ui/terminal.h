/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_UI_TERMINAL_H_
#define INCLUDE_UI_TERMINAL_H_

#include <curses.h>

#include <memory>
#include <vector>

#include "ui/block.h"

namespace interface {

class Terminal {
 public:
  // Constructor
  Terminal() : win_(), max_column_(0), max_row_(0), blocks_(), exit_(false){};

  // Destructor
  virtual ~Terminal() { Destroy(); };

  // Remove these operators
  Terminal(const Terminal& other) = delete;             // copy constructor
  Terminal(Terminal&& other) = delete;                  // move constructor
  Terminal& operator=(const Terminal& other) = delete;  // copy assignment
  Terminal& operator=(Terminal&& other) = delete;       // move assignment

  int Init();
  int Destroy();

  void AppendBlock(std::unique_ptr<Block>& b);

  void PollingInput();

  bool Tick();

  bool IsDimensionUpdated();
  void Draw();

 private:
  WINDOW* win_;
  short max_column_, max_row_;

  std::vector<std::unique_ptr<Block>> blocks_;

  bool exit_;
};

}  // namespace interface
#endif  // INCLUDE_UI_TERMINAL_H_