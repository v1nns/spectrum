#include "ui/base/terminal.h"

#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <functional>
#include <iostream>

#include "ftxui/component/captured_mouse.hpp"      // for ftxui
#include "ftxui/component/component.hpp"           // for Menu
#include "ftxui/component/component_options.hpp"   // for MenuOption
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "ui/block/list_directory.h"

namespace interface {

/* ********************************************************************************************** */

Terminal::Terminal() : container_(nullptr){};

/* ********************************************************************************************** */

Terminal::~Terminal(){};

/* ********************************************************************************************** */

void Terminal::Init() {
  //   container_ = Container::Horizontal({Make<ListDirectory>()});
  container_ = Make<ListDirectory>();
}

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

  //   container_ = CatchEvent(container_, [&](Event event) {
  //     if (event == Event::Character('q')) {
  //       screen.ExitLoopClosure();
  //       return true;
  //     }
  //     return false;
  //   });

  screen.Loop(container_);
}

}  // namespace interface