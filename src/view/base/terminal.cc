
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

  // As this terminal will hold all these interface blocks, there is nothing better than
  // use it as a mediator to send events between them
  std::shared_ptr<EventDispatcher> dispatcher = shared_from_this();

  // Create controllers
  player_ = std::make_shared<controller::Player>(dispatcher);

  // Create blocks
  auto list_dir = std::make_shared<ListDirectory>(dispatcher, custom_path);
  auto file_info = std::make_shared<FileInfo>(dispatcher);
  auto audio_player = std::make_shared<AudioPlayer>(dispatcher);

  // Attach controller as listener to block actions
  list_dir->Attach(std::static_pointer_cast<ActionListener>(player_));

  // Make every block as a child of this terminal
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
    // TODO: this is an error, should exit...
    return ftxui::text("Empty container");
  }

  ftxui::Element list_dir = children_.at(0)->Render();
  ftxui::Element file_info = children_.at(1)->Render();
  ftxui::Element spectrum_graph = ftxui::filler() | ftxui::border;
  ftxui::Element audio_player = children_.at(2)->Render();

  ftxui::Element terminal = ftxui::hbox({
      ftxui::vbox({std::move(list_dir), std::move(file_info)}),
      ftxui::vbox({std::move(spectrum_graph), std::move(audio_player)}) | ftxui::xflex_grow,
  });

  ftxui::Element error = ftxui::text("");

  if (last_error_) {
    std::string message(error::ApplicationError::GetMessage(last_error_.value()));

    using ftxui::WIDTH, ftxui::HEIGHT, ftxui::EQUAL;
    // TODO: split this to an element (derived from Node)
    error = ftxui::vbox({
                ftxui::text(" ERROR") | ftxui::bold,
                ftxui::text(""),
                ftxui::paragraph(message) | ftxui::center | ftxui::bold,
            }) |
            ftxui::bgcolor(ftxui::Color::DarkRedBis) | ftxui::size(HEIGHT, EQUAL, 5) |
            ftxui::size(WIDTH, EQUAL, 35) | ftxui::borderDouble |
            ftxui::color(ftxui::Color::Grey93) | ftxui::center;
  }

  return ftxui::dbox({terminal, error});
}

/* ********************************************************************************************** */

bool Terminal::OnEvent(ftxui::Event event) {
  if (last_error_ && OnErrorModeEvent(event)) return true;

  if (OnGlobalModeEvent(event)) return true;

  for (auto& child : children_) {
    if (child->OnEvent(event)) return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool Terminal::OnGlobalModeEvent(ftxui::Event event) {
  // Exit application
  if (event == ftxui::Event::Character('q')) {
    Exit();
    return true;
  }

  // Clear current song from player controller
  if (event == ftxui::Event::Character('c')) {
    player_->ClearCurrentSong();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool Terminal::OnErrorModeEvent(ftxui::Event event) {
  if (event == ftxui::Event::Return | event == ftxui::Event::Escape |
      event == ftxui::Event::Character('q')) {
    last_error_.reset();
  }

  // This is to ensure that no one else will treat any event while on error mode
  return true;
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

/* ********************************************************************************************** */

void Terminal::SendTo(BlockIdentifier id, BlockEvent event) {
  for (auto& child : children_) {
    auto block = std::static_pointer_cast<Block>(child);
    if (block->GetId() == id) {
      block->OnBlockEvent(event);
      break;
    }
  }
}

/* ********************************************************************************************** */

void Terminal::SetApplicationError(error::Code id) { last_error_ = id; }

}  // namespace interface