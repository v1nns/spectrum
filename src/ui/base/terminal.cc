
#include "ui/base/terminal.h"

#include <stdlib.h>  // for exit, EXIT_FAILURE

#include <functional>  // for function

#include "ftxui/component/component.hpp"           // for CatchEvent, Make
#include "ftxui/component/event.hpp"               // for Event
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "ui/block/list_directory.h"               // for ListDirectory

namespace interface {

/* ********************************************************************************************** */

Terminal::Terminal() : container_(nullptr){};

/* ********************************************************************************************** */

Terminal::~Terminal(){};

/* ********************************************************************************************** */

void Terminal::Init() { container_ = Make<ListDirectory>(); }

/* ********************************************************************************************** */

void Terminal::Exit() {
  //   if (critical_error_) {
  //     std::cerr << "error: " << critical_error_->second << std::endl;
  //   }

  std::exit(EXIT_FAILURE);
}

/* ********************************************************************************************** */

void Terminal::Loop() {
  auto screen = ScreenInteractive::Fullscreen();

  // Handle events and run global commands
  container_ = CatchEvent(container_, [&](Event event) {
    if (event == Event::Character('q')) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  screen.Loop(container_);
}

}  // namespace interface