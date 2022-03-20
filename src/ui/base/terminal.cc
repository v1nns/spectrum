#include "ui/base/terminal.h"

#include <unistd.h>

#include <cctype>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>

#include "ui/colors.h"

namespace interface {

constexpr int kDelayLoop = 5000;  //!< Delay to handle keyboard input and refresh screen

bool Terminal::resize_screen_ = false;

/* ********************************************************************************************** */

Terminal::Terminal()
    : max_size_({0, 0}), blocks_(), has_focus_(true), critical_error_(), exit_(false){};

/* ********************************************************************************************** */

Terminal::~Terminal() { Destroy(); };

/* ********************************************************************************************** */

void Terminal::Init() {
  // Initialize terminal window (a.k.a. "stdscr" in ncurses)
  if (initscr() == nullptr) {
    SetCriticalError(error::kTerminalInitialization);
    Exit();
  }

  // Check color availability
  if (!has_colors() || !can_change_color()) {
    SetCriticalError(error::kTerminalColorsUnavailable);
    Exit();
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

  // Register hook to watch for received signals
  signal(SIGWINCH, Terminal::SignalHook);
}

/* ********************************************************************************************** */

void Terminal::Destroy() {
  // Destroy windows from all blocks
  for (auto& block : blocks_) {
    block->Destroy();
  }

  // Delete terminal window
  endwin();
  refresh();
}

/* ********************************************************************************************** */

void Terminal::Exit() {
  if (critical_error_) {
    std::cerr << "error: " << critical_error_->second << std::endl;
  }

  std::exit(EXIT_FAILURE);
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

void Terminal::SetCriticalError(int err_code) {
  error::ErrorTable table;
  critical_error_ = table.GetMessage(err_code);
  exit_ = true;
}

/* ********************************************************************************************** */

void Terminal::SetFocus(bool focused) { has_focus_ = !focused; }

/* ********************************************************************************************** */

void Terminal::AppendBlock(std::unique_ptr<Block>& block) {
  // TODO: add size control here with assert
  block->Init(max_size_);

  Callbacks cbs{.set_error = std::bind(&Terminal::SetCriticalError, this, std::placeholders::_1),
                .set_focus = std::bind(&Terminal::SetFocus, this, std::placeholders::_1)};

  block->RegisterCallbacks(std::move(cbs));
  blocks_.push_back(std::move(block));
}

/* ********************************************************************************************** */

bool Terminal::Tick() {
  if (resize_screen_) {
    // Resize and reset internal flag
    OnResize();
    resize_screen_ = false;
  } else {
    OnPolling();
  }

  OnDraw();

  if (critical_error_) Exit();

  usleep(kDelayLoop);
  return !exit_;
}

/* ********************************************************************************************** */

screen_size_t Terminal::GetScreenSize() {
  screen_size_t size{0, 0};
  getmaxyx(stdscr, size.row, size.column);

  return size;
}

/* ********************************************************************************************** */

void Terminal::SignalHook(int sig) {
  // In case it is a resize event, must set related flag
  if (sig == SIGWINCH) resize_screen_ = true;

#if defined(__sun) && defined(__SVR4)
  // reinstall the hook each time it's executed
  signal(sig, global_sig_hook);
#endif  // __sun && __SVR4
}

}  // namespace interface