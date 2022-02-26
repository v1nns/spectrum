#include "ui/block.h"

#include <assert.h>

namespace interface {

Block::Block(const point_t& init, const screen_size_t& size, const std::string& title)
    : init_(init), size_(size), border_(), win_(), first_draw_(true), border_title_(title) {}

void Block::Init(short max_row, short max_column) {
  assert((init_.y + size_.row) <= max_row);
  assert((init_.x + size_.column) <= max_column);

  border_ = newwin(size_.row, size_.column, init_.y, init_.x);
  assert(border_ != NULL);

  win_ = newwin(size_.row - 2, size_.column - 2, init_.y + 1, init_.x + 1);
  assert(win_ != NULL);
}

void Block::Destroy() {
  delwin(border_);
  delwin(win_);
}

void Block::RecreateWindow(short max_row, short max_column) {
  Destroy();
  Init(max_row, max_column);
}

void Block::DrawBorder() {
  // Erase content from window and redraw border
  werase(border_);
  box(border_, 0, 0);

  // Write title overwriting the border and refresh window
  mvwprintw(border_, 0, 1, border_title_.c_str());
  wrefresh(border_);
}

}  // namespace interface
