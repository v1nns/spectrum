
#include "view/base/terminal.h"

#include <stdlib.h>  // for exit, EXIT_FAILURE

#include <functional>  // for function
#include <utility>     // for move

#include "ftxui/component/component.hpp"           // for CatchEvent, Make
#include "ftxui/component/event.hpp"               // for Event
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "view/base/block.h"                       // for Block, BlockEvent
#include "view/block/audio_player.h"
#include "view/block/file_info.h"       // for FileInfo
#include "view/block/list_directory.h"  // for ListDirectory

namespace interface {

/* ********************************************************************************************** */

Terminal::Terminal()
    : EventDispatcher(), ftxui::ComponentBase(), player_(nullptr), cb_exit_(nullptr) {}

/* ********************************************************************************************** */

Terminal::~Terminal() {
  cb_exit_ = nullptr;
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
  auto audio_player = std::make_shared<AudioPlayer>(shared_from_this());

  // Attach controller as listener to block actions
  list_dir->Attach(std::static_pointer_cast<ActionListener>(player_));

  Add(list_dir);
  Add(file_info);
  Add(audio_player);
}

/* ********************************************************************************************** */

void Terminal::Exit() {
  //   if (critical_error_) {
  //     std::cerr << "error: " << critical_error_->second << std::endl;
  //     std::exit(EXIT_FAILURE);
  //   }

  if (cb_exit_ != nullptr) {
    cb_exit_();
  }
}

/* ********************************************************************************************** */

void Terminal::RegisterExitCallback(Callback cb) { cb_exit_ = cb; }

/* ********************************************************************************************** */

ftxui::Element Terminal::Render() {
  if (children_.size() == 0) {
    return ftxui::text("Empty container");
  }

  ftxui::Element list_dir = children_.at(0)->Render();
  ftxui::Element file_info = children_.at(1)->Render();
  ftxui::Element spectrum_graph = ftxui::filler() | ftxui::border;
  ftxui::Element audio_player = children_.at(2)->Render();

  return ftxui::hbox({
             ftxui::vbox({std::move(list_dir), std::move(file_info)}),
             ftxui::vbox({std::move(spectrum_graph), std::move(audio_player)}) | ftxui::xflex_grow,
         }) |
         ftxui::flex_grow;
}

/* ********************************************************************************************** */

bool Terminal::OnEvent(ftxui::Event event) {
  // TODO: create a OnGlobalEvent
  if (event == ftxui::Event::Character('q')) {
    Exit();
    return true;
  }
  for (ftxui::Component& child : children_) {
    if (child->OnEvent(event)) return true;
  }
  return false;
}

/* ********************************************************************************************** */

void Terminal::Broadcast(Block* sender, BlockEvent event) {
  for (auto& child : children_) {
    auto block = std::static_pointer_cast<Block>(child);
    if (sender == nullptr || block->GetId() != sender->GetId()) {
      block->OnBlockEvent(event);
    }
  }
}

}  // namespace interface