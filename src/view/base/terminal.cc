
#include "view/base/terminal.h"

#include <stdlib.h>  // for exit, EXIT_FAILURE

#include <functional>  // for function
#include <utility>     // for move

#include "ftxui/component/component.hpp"           // for CatchEvent, Make
#include "ftxui/component/event.hpp"               // for Event
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "view/base/block.h"                       // for Block, BlockEvent
#include "view/block/audio_visualizer.h"
#include "view/block/file_info.h"       // for FileInfo
#include "view/block/list_directory.h"  // for ListDirectory
#include "view/block/media_player.h"

namespace interface {

std::shared_ptr<Terminal> Terminal::Create() {
  // Simply extend the Terminal class, as we do not want to expose the default constructor, neither
  // do we want to use std::make_shared explicitly calling operator new()
  struct MakeSharedEnabler : public Terminal {};
  auto terminal = std::make_shared<MakeSharedEnabler>();

  // Initialize internal components
  terminal->Init();

  return terminal;
}

/* ********************************************************************************************** */

Terminal::Terminal()
    : EventDispatcher{},
      ftxui::ComponentBase{},
      listener_{},
      receiver_{ftxui::MakeReceiver<CustomEvent>()},
      sender_{receiver_->MakeSender()},
      cb_send_event_{},
      cb_exit_{} {}

/* ********************************************************************************************** */

Terminal::~Terminal() {
  // Base class will do the rest (release resources by detaching all blocks, a.k.a. children)
}

/* ********************************************************************************************** */

void Terminal::Init() {
  // TODO: remove this after developing
  std::string custom_path = "/home/vinicius/Downloads";

  // As this terminal will hold all these interface blocks, there is nothing better than
  // use itself as a mediator to send events between them
  std::shared_ptr<EventDispatcher> dispatcher = shared_from_this();

  // Create blocks
  auto list_dir = std::make_shared<ListDirectory>(dispatcher, custom_path);
  auto file_info = std::make_shared<FileInfo>(dispatcher);
  auto audio_visualizer = std::make_shared<AudioVisualizer>(dispatcher);
  auto media_player = std::make_shared<MediaPlayer>(dispatcher);

  // Make every block as a child of this terminal
  Add(list_dir);
  Add(file_info);
  Add(audio_visualizer);
  Add(media_player);
}

/* ********************************************************************************************** */

void Terminal::Exit() {
  //   if (critical_error_) {
  //     std::cerr << "error: " << critical_error_->second << std::endl;
  //     std::exit(EXIT_FAILURE);
  //   }

  // Trigger exit callback
  if (cb_exit_ != nullptr) {
    cb_exit_();
  }
}

/* ********************************************************************************************** */

void Terminal::RegisterInterfaceListener(const std::shared_ptr<interface::Listener>& listener) {
  listener_ = listener;
}

/* ********************************************************************************************** */

void Terminal::RegisterEventSenderCallback(EventCallback cb) { cb_send_event_ = cb; }

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
  ftxui::Element audio_visualizer = children_.at(2)->Render();
  ftxui::Element audio_player = children_.at(3)->Render();

  ftxui::Element terminal = ftxui::hbox({
      ftxui::vbox({list_dir, file_info}),
      ftxui::vbox({audio_visualizer, audio_player}) | ftxui::xflex_grow,
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

  if (OnCustomEvent()) return true;

  if (OnGlobalModeEvent(event)) return true;

  for (auto& child : children_) {
    if (child->OnEvent(event)) return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool Terminal::OnCustomEvent() {
  if (!receiver_->HasPending()) return false;

  CustomEvent event;
  if (!receiver_->Receive(&event)) return false;

  // As this class centralizes any event sending (to an external listener or some child block),
  // first gotta check if this event is specifically for the outside listener
  if (event.type == CustomEvent::Type::FromInterfaceToAudioThread) {
    auto media_ctl = listener_.lock();
    if(!media_ctl) return true; // TODO: improve handling

    switch (event.id) {
      case CustomEvent::Identifier::NotifyFileSelection: {
        auto content = event.GetContent<std::filesystem::path>();
        media_ctl->NotifyFileSelection(content);
      } break;

      case CustomEvent::Identifier::PauseOrResumeSong:
        media_ctl->PauseOrResume();
        break;

      case CustomEvent::Identifier::ClearCurrentSong:
        media_ctl->ClearCurrentSong();
        break;

      default:
        break;
    }

    return true;
  }

  // Otherwise, send it to children blocks
  for (auto& child : children_) {
    auto block = std::static_pointer_cast<Block>(child);
    if (block->OnCustomEvent(event)) return true;
  }

  return false;
}

/* **********************************************************************************************
 */

bool Terminal::OnGlobalModeEvent(const ftxui::Event& event) {
  // Exit application
  if (event == ftxui::Event::Character('q')) {
    Exit();
    return true;
  }

  return false;
}

/* **********************************************************************************************
 */

bool Terminal::OnErrorModeEvent(const ftxui::Event& event) {
  if (event == ftxui::Event::Return || event == ftxui::Event::Escape ||
      event == ftxui::Event::Character('q')) {
    last_error_.reset();
  }

  // This is to ensure that no one else will treat any event while on error mode
  return true;
}

/* **********************************************************************************************
 */

void Terminal::SendEvent(const CustomEvent& event) {
  sender_->Send(event);
  cb_send_event_(ftxui::Event::Custom);  // force a refresh
}

/* **********************************************************************************************
 */

void Terminal::SetApplicationError(error::Code id) { last_error_ = id; }

}  // namespace interface
