#include "ui/base/block.h"

#include <assert.h>

namespace interface {

Block::Block(const screen_portion_t& init, const screen_portion_t& size, const std::string& title,
             State* state)
    : init_(init),
      size_(size),
      calc_init_(),
      calc_size_(),
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
  // Calculate initial coordinate
  calc_init_.x = init_.column * max_size.column;
  calc_init_.y = init_.row * max_size.row;

  // Calculate block size based on a screen portion
  calc_size_.column = size_.column * max_size.column;
  calc_size_.row = size_.row * max_size.row;

  assert((calc_init_.x + calc_size_.column) <= max_size.column);
  assert((calc_init_.y + calc_size_.row) <= max_size.row);

  border_ = newwin(calc_size_.row, calc_size_.column, calc_init_.y, calc_init_.x);
  assert(border_ != NULL);

  win_ = newwin(calc_size_.row - 2, calc_size_.column - 2, calc_init_.y + 1, calc_init_.x + 1);
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
  // Calculate initial coordinate
  calc_init_.x = init_.column * max_size.column;
  calc_init_.y = init_.row * max_size.row;

  // Calculate block size based on a screen portion
  calc_size_.column = size_.column * max_size.column;
  calc_size_.row = size_.row * max_size.column;

  assert((calc_init_.x + calc_size_.column) <= max_size.column);
  assert((calc_init_.y + calc_size_.row) <= max_size.row);

  // Resize window
  wresize(border_, calc_size_.row, calc_size_.column);
  wresize(win_, calc_size_.row - 2, calc_size_.column - 2);

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

void Block::ChangeState(State* new_state) {
  if (curr_state_ != nullptr) {
    delete curr_state_;
  }
  curr_state_ = new_state;
  refresh_ = true;
}

}  // namespace interface
