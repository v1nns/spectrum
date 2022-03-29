
#include "ui/base/terminal.h"

#include <stdlib.h>  // for exit, EXIT_FAILURE

#include <functional>  // for function

#include "ftxui/component/component.hpp"           // for CatchEvent, Make
#include "ftxui/component/event.hpp"               // for Event
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "ui/block/file_info.h"
#include "ui/block/list_directory.h"  // for ListDirectory

namespace interface {

/* ********************************************************************************************** */

Terminal::Terminal() : container_(nullptr){};

/* ********************************************************************************************** */

Terminal::~Terminal(){};

/* ********************************************************************************************** */

void Terminal::Init() {
  auto block1 = Make<ListDirectory>();
  auto block2 = Make<FileInfo>();

  container_ = Container::Vertical({
      std::move(block1),
      std::move(block2),
  });
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

  // DEBUG (idea of first "final" UI version)
  //   container_.reset();
  //   container_ = Renderer([&]() {
  //     return hflow({
  //         vflow({
  //             window(text(" block1 ") | bold, text("dummy") | dim) | flex,
  //             window(text(" block3 ") | bold, text("dummy3")) | xflex | size(HEIGHT, EQUAL, 15),
  //         }) | size(WIDTH, GREATER_THAN, 28),
  //         window(text(" block2 ") | bold, text("dummy2") | center) | flex,
  //     });
  //   });
  // END DEBUG

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