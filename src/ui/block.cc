#include "ui/block.h"

#include <assert.h>

namespace interface {

Block::Block(const point_t& init, const screen_size_t& size, const std::string& title,
             BlockState* state)
    : init_(init),
      size_(size),
      border_(),
      win_(),
      border_title_(title),
      state_(state),
      refresh_(true) {}

/* ********************************************************************************************** */

Block::~Block() {
  if (state_ != nullptr) {
    delete state_;
  }
}

/* ********************************************************************************************** */

void Block::Init(const screen_size_t& max_size) {
  //   size_ = max_size;
  //   size_.column -= 2;
  //   size_.row -= 2;

  assert((init_.y + size_.row) <= max_size.row);
  assert((init_.x + size_.column) <= max_size.column);

  border_ = newwin(size_.row, size_.column, init_.y, init_.x);
  assert(border_ != NULL);

  win_ = newwin(size_.row - 2, size_.column - 2, init_.y + 1, init_.x + 1);
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
  // TODO: think about it "wresize"
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

    state_->Draw(*this);
    refresh_ = false;
  }
}

/* ********************************************************************************************** */

void Block::HandleInput(char key) { state_->HandleInput(*this, key); }

/* ********************************************************************************************** */

void Block::ChangeState(BlockState* new_state) {
  if (state_ != nullptr) {
    delete state_;
  }
  state_ = new_state;
  refresh_ = true;
}

}  // namespace interface
