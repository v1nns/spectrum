
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

Terminal::Terminal() : Dispatcher(), container_(nullptr){};

/* ********************************************************************************************** */

Terminal::~Terminal() {
  for (auto& block : blocks_) {
    block.reset();
  }
};

/* ********************************************************************************************** */

void Terminal::Init() {
  auto list_dir = Make<ListDirectory>(shared_from_this());
  auto file_info = Make<FileInfo>(shared_from_this());

  Add(list_dir);
  Add(file_info);

  container_ = Container::Vertical({blocks_.at(0), blocks_.at(1)});
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

/* ********************************************************************************************** */

void Terminal::Add(const std::shared_ptr<Block>& b) { blocks_.push_back(std::move(b)); }

/* ********************************************************************************************** */

void Terminal::Broadcast(Block* sender, BlockEvent event) {
  for (auto& block : blocks_) {
    if (block->GetId() != sender->GetId()) {
      block->OnBlockEvent(event);
    }
  }
}

}  // namespace interface