
#include "view/base/terminal.h"

#include <stdlib.h>  // for exit, EXIT_FAILURE

#include <functional>  // for function
#include <utility>     // for move

#include "ftxui/component/component.hpp"           // for CatchEvent, Make
#include "ftxui/component/event.hpp"               // for Event
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "view/base/block.h"                       // for Block, BlockEvent
#include "view/block/file_info.h"                  // for FileInfo
#include "view/block/list_directory.h"             // for ListDirectory

namespace interface {

/* ********************************************************************************************** */

Terminal::Terminal() : EventDispatcher(), player_(nullptr), blocks_(), container_(nullptr) {}

/* ********************************************************************************************** */

Terminal::~Terminal() {
  container_.reset();
  blocks_.clear();
  player_.reset();
}

/* ********************************************************************************************** */

void Terminal::Init() {
  // TODO: remove this after developing
  std::string custom_path = "/home/vinicius/projects/music-analyzer/";

  // Create controllers
  player_ = std::make_shared<controller::Player>(shared_from_this());

  // Create blocks
  auto list_dir = std::make_shared<ListDirectory>(shared_from_this(), custom_path);
  auto file_info = std::make_shared<FileInfo>(shared_from_this());

  // Attach controller as listener to block actions
  list_dir->Attach(std::static_pointer_cast<ActionListener>(player_));

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

  // DEBUG (idea of first "final" view version)
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