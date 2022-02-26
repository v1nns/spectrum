/**
 * \file
 * \brief  Base class for a block in UI
 */

#ifndef INCLUDE_UI_BLOCK_H_
#define INCLUDE_UI_BLOCK_H_

#include <curses.h>

#include <string>

#include "ui/common.h"

namespace interface {

class Block {
 protected:
  // Constructor (cannot instantiate directly, only derived class can use it)
  explicit Block(const point_t& init, const screen_size_t& size, const std::string& title);

 public:
  void Init(short max_row, short max_column);
  void Destroy();
  void RecreateWindow(short max_row, short max_column);

  void DrawBorder();

  // Implementation must be made by derived class
  virtual void Draw(bool rescale) = 0;
  virtual void HandleInput(char key) = 0;

 protected:
  point_t init_;
  screen_size_t size_;

  WINDOW *border_, *win_;

  bool first_draw_;
  std::string border_title_;
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_H_