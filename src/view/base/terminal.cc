
#include "view/base/terminal.h"

#include <stdlib.h>  // for exit, EXIT_FAILURE

#include <cmath>
#include <functional>  // for function
#include <memory>
#include <utility>  // for move

#include "ftxui/component/component.hpp"           // for CatchEvent, Make
#include "ftxui/component/event.hpp"               // for Event
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "ftxui/screen/terminal.hpp"
#include "view/base/block.h"  // for Block, BlockEvent
#include "view/block/audio_visualizer.h"
#include "view/block/file_info.h"       // for FileInfo
#include "view/block/list_directory.h"  // for ListDirectory
#include "view/block/media_player.h"

namespace interface {

//! To make life easier
bool operator!=(const ftxui::Dimensions& lhs, const ftxui::Dimensions& rhs) {
  return std::tie(lhs.dimx, lhs.dimy) != std::tie(rhs.dimx, rhs.dimy);
}

/* ********************************************************************************************** */

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
      last_error_{error::kSuccess},
      error_dialog_{std::make_unique<Dialog>()},
      receiver_{ftxui::MakeReceiver<CustomEvent>()},
      sender_{receiver_->MakeSender()},
      cb_send_event_{},
      cb_exit_{},
      size_{ftxui::Terminal::Size()} {}

/* ********************************************************************************************** */

Terminal::~Terminal() {
  // Base class will do the rest (release resources by detaching all blocks, a.k.a. children)
}

/* ********************************************************************************************** */

void Terminal::Init() {
  // TODO: remove this after developing
  std::string custom_path = "/home/vinicius/Downloads/music";

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

void Terminal::RegisterEventSenderCallback(EventCallback cb) {
  cb_send_event_ = cb;
  cb_send_event_(ftxui::Event::Custom);  // force a refresh to handle any pending custom event
                                         // (update UI with volume information)
}

/* ********************************************************************************************** */

void Terminal::RegisterExitCallback(Callback cb) { cb_exit_ = cb; }

/* ********************************************************************************************** */

ftxui::Element Terminal::Render() {
  if (children_.size() == 0 || children_.size() != 4) {
    // TODO: this is an error, should exit...
    return ftxui::text("Empty container");
  }

  // Check if terminal has been resized
  auto current_size = ftxui::Terminal::Size();
  if (size_ != current_size) {
    size_ = current_size;

    // Recalculate maximum number of bars to show in spectrum graphic
    int number_bars = CalculateNumberBars();

    // Send event to audio analysis thread
    auto event_resize = interface::CustomEvent::ResizeAnalysis(number_bars);
    SendEvent(event_resize);
  }

  // Render each block
  ftxui::Element list_dir = children_.at(0)->Render();
  ftxui::Element file_info = children_.at(1)->Render();
  ftxui::Element audio_visualizer = children_.at(2)->Render();
  ftxui::Element audio_player = children_.at(3)->Render();

  // Glue everything together
  ftxui::Element terminal = ftxui::hbox({
      ftxui::vbox({std::move(list_dir), std::move(file_info)}),
      ftxui::vbox({std::move(audio_visualizer), std::move(audio_player)}) | ftxui::xflex_grow,
  });

  // Render dialog box
  ftxui::Element error = error_dialog_->IsVisible() ? error_dialog_->Render() : ftxui::text("");

  return ftxui::dbox({terminal, error});
}

/* ********************************************************************************************** */

bool Terminal::OnEvent(ftxui::Event event) {
  // Cannot do anything while dialog box is opened
  if (error_dialog_->IsVisible()) return error_dialog_->OnEvent(event);

  // Treat any pending custom event
  OnCustomEvent();

  if (OnGlobalModeEvent(event)) return true;

  for (auto& child : children_) {
    if (child->OnEvent(event)) return true;
  }

  return false;
}

/* ********************************************************************************************** */

int Terminal::CalculateNumberBars() {
  // In this case, should calculate new size for audio visualizer (number of bars for spectrum)
  auto block = std::static_pointer_cast<Block>(children_.at(0));

  // crazy math function = (a - b - c - d) / e;
  // considering these:
  // a = terminal maximum width
  // b = ListDirectory width
  // c = border characters
  // d = some constant to represent spacing between bars
  // e = bar thickness + 1
  float crazy_math = ((float)(size_.dimx - block->GetSize().width - 2 - 10) / 4);
  crazy_math += crazy_math * .01;

  // Round to nearest odd number
  int number_bars;
  if ((int)floor(crazy_math) % 2 == 0)
    number_bars = floor(crazy_math);
  else
    number_bars = ceil(crazy_math);

  return number_bars;
}

/* ********************************************************************************************** */

void Terminal::OnCustomEvent() {
  while (receiver_->HasPending()) {
    CustomEvent event;
    if (!receiver_->Receive(&event)) break;

    // As this class centralizes any event sending (to an external listener or some child block),
    // first gotta check if this event is specifically for the outside listener
    if (event.type == CustomEvent::Type::FromInterfaceToAudioThread) {
      auto media_ctl = listener_.lock();
      if (!media_ctl) {
        // TODO: improve handling here and also for each method call
        continue;  // skip to next while-loop
      }

      switch (event.id) {
        case CustomEvent::Identifier::NotifyFileSelection: {
          auto content = event.GetContent<std::filesystem::path>();
          media_ctl->NotifyFileSelection(content);
        } break;

        case CustomEvent::Identifier::PauseOrResumeSong:
          media_ctl->PauseOrResume();
          break;

        case CustomEvent::Identifier::StopSong:
          media_ctl->Stop();
          break;

        case CustomEvent::Identifier::ClearCurrentSong:
          media_ctl->ClearCurrentSong();
          break;

        case CustomEvent::Identifier::SetAudioVolume: {
          auto content = event.GetContent<model::Volume>();
          media_ctl->SetVolume(content);
        } break;

        case CustomEvent::Identifier::ResizeAnalysis: {
          auto content = event.GetContent<int>();
          media_ctl->ResizeAnalysisOutput(content);
        } break;

        case CustomEvent::Identifier::SeekForwardPosition: {
          auto content = event.GetContent<int>();
          media_ctl->SeekForwardPosition(content);
        } break;

        case CustomEvent::Identifier::SeekBackwardPosition: {
          auto content = event.GetContent<int>();
          media_ctl->SeekBackwardPosition(content);
        } break;

        default:
          break;
      }

      continue;  // skip to next while-loop
    }

    // Otherwise, send it to children blocks
    for (auto& child : children_) {
      auto block = std::static_pointer_cast<Block>(child);
      if (block->OnCustomEvent(event)) {
        break;  // skip from this for-loop
      }
    }
  }
}

/* ********************************************************************************************** */

bool Terminal::OnGlobalModeEvent(const ftxui::Event& event) {
  // Exit application
  if (event == ftxui::Event::Character('q')) {
    Exit();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void Terminal::SendEvent(const CustomEvent& event) {
  sender_->Send(event);
  cb_send_event_(ftxui::Event::Custom);  // force a refresh
}

/* ********************************************************************************************** */

void Terminal::ProcessEvent(const CustomEvent& event) {
  // This method was planned to execute any custom event while Screen loop is not running yet
  sender_->Send(event);
  OnCustomEvent();
}

/* ********************************************************************************************** */

void Terminal::SetApplicationError(error::Code id) {
  std::string message{error::ApplicationError::GetMessage(id)};
  error_dialog_->SetErrorMessage(message);

  last_error_ = id;
}

}  // namespace interface
