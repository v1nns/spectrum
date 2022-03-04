#include "ui/block.h"

#include <assert.h>

namespace interface {

Block::Block(const point_t& init, const screen_portion_t& size, const std::string& title,
             BlockState* state)
    : init_(init),
      size_(size),
      border_(),
      win_(),
      border_title_(title),
      curr_state_(state),
      refresh_(true) {}

/* ********************************************************************************************** */

Block::~Block() {
  if (curr_state_ != nullptr) {
    delete curr_state_;
  }
}

/* ********************************************************************************************** */

void Block::Init(const screen_size_t& max_size) {
  // Calculate screen proportion
  short column = size_.column * max_size.column;
  short row = size_.row * max_size.row;

  assert((init_.x + column) <= max_size.column);
  assert((init_.y + row) <= max_size.row);

  border_ = newwin(row, column, init_.y, init_.x);
  assert(border_ != NULL);

  win_ = newwin(row - 2, column - 2, init_.y + 1, init_.x + 1);
  assert(win_ != NULL);
}

/* ********************************************************************************************** */

void Block::Destroy() {
  delwin(border_);
  delwin(win_);
  refresh();
  clear();
}

/* ********************************************************************************************** */

void Block::ResizeWindow(const screen_size_t& max_size) {
  // Calculate screen proportion
  short column = size_.column * max_size.column;
  short row = size_.row * max_size.row;

  assert((init_.x + column) <= max_size.column);
  assert((init_.y + row) <= max_size.row);

  // Resize window
  wresize(border_, row, column);
  wresize(win_, row - 2, column - 2);

  // Force to refresh UI on next "Draw" calling
  refresh_ = true;
}

/* ********************************************************************************************** */

void Block::DrawBorder() {
  // Erase content from window and redraw border
  werase(border_);
  box(border_, 0, 0);

  // Write title overwriting the border and refresh window
  mvwprintw(border_, 0, 1, border_title_.c_str());
  wrefresh(border_);
}

/* ********************************************************************************************** */

void Block::Draw() {
  if (refresh_) {
    DrawBorder();

    curr_state_->Draw(*this);
    refresh_ = false;
  }
}

/* ********************************************************************************************** */

void Block::HandleInput(char key) { curr_state_->HandleInput(*this, key); }

/* ********************************************************************************************** */

void Block::ChangeState(BlockState* new_state) {
  if (curr_state_ != nullptr) {
    delete curr_state_;
  }
  curr_state_ = new_state;
  refresh_ = true;
}

}  // namespace interface
