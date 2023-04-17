
#include "view/base/terminal.h"

#include <stdlib.h>  // for exit, EXIT_FAILURE

#include <cmath>
#include <functional>  // for function
#include <memory>
#include <set>
#include <utility>  // for move

#include "ftxui/component/component.hpp"           // for CatchEvent, Make
#include "ftxui/component/event.hpp"               // for Event
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "ftxui/screen/terminal.hpp"
#include "model/bar_animation.h"
#include "util/logger.h"
#include "view/base/block.h"
#include "view/block/file_info.h"
#include "view/block/list_directory.h"
#include "view/block/media_player.h"
#include "view/block/tab_viewer.h"

namespace interface {

//! To make life easier
bool operator!=(const ftxui::Dimensions& lhs, const ftxui::Dimensions& rhs) {
  return std::tie(lhs.dimx, lhs.dimy) != std::tie(rhs.dimx, rhs.dimy);
}

/* ********************************************************************************************** */

std::shared_ptr<Terminal> Terminal::Create() {
  LOG("Create new instance of terminal");

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
    : EventDispatcher{}, ftxui::ComponentBase{}, sender_{receiver_->MakeSender()} {}

/* ********************************************************************************************** */

void Terminal::Init() {
  LOG("Initialize terminal");

  // As this terminal will hold all these interface blocks, there is nothing better than
  // use itself as a mediator to send events between them
  std::shared_ptr<EventDispatcher> dispatcher = shared_from_this();

  // Create blocks
  auto list_dir = std::make_shared<ListDirectory>(dispatcher);
  auto file_info = std::make_shared<FileInfo>(dispatcher);
  auto tab_viewer = std::make_shared<TabViewer>(dispatcher);
  auto media_player = std::make_shared<MediaPlayer>(dispatcher);

  // As default, make ListDirectory focused to receive input commands
  list_dir->SetFocused(true);

  // Make every block as a child of this terminal
  Add(list_dir);
  Add(file_info);
  Add(tab_viewer);
  Add(media_player);
}

/* ********************************************************************************************** */

void Terminal::Exit() const {
  LOG("Exit from terminal");

  // Trigger exit callback
  if (cb_exit_ != nullptr) {
    cb_exit_();
  }
}

/* ********************************************************************************************** */

void Terminal::RegisterPlayerNotifier(const std::shared_ptr<audio::Notifier>& notifier) {
  notifier_ = notifier;
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
  if (children_.empty() || children_.size() != 4) {
    // TODO: this is an error, should exit...
    return ftxui::text("Empty container");
  }

  // Check if terminal has been resized
  if (auto current_size = ftxui::Terminal::Size(); size_ != current_size) {
    LOG("Resize terminal size with new value=(x:", current_size.dimx, "y:", current_size.dimy, ")");
    size_ = current_size;

    // Recalculate maximum number of bars to show in spectrum graphic
    int number_bars = CalculateNumberBars();

    // Send value to spectrum visualizer
    auto event_calculate = CustomEvent::CalculateNumberOfBars(number_bars);
    SendEvent(event_calculate);
  }

  // Render each block
  ftxui::Element list_dir = children_.at(0)->Render();
  ftxui::Element file_info = children_.at(1)->Render();
  ftxui::Element tab_viewer = children_.at(2)->Render();
  ftxui::Element audio_player = children_.at(3)->Render();

  // Glue everything together
  ftxui::Element terminal = ftxui::hbox({
      ftxui::vbox({std::move(list_dir), std::move(file_info)}),
      ftxui::vbox({std::move(tab_viewer), std::move(audio_player)}) | ftxui::xflex_grow,
  });

  // Render dialog box as overlay
  ftxui::Element overlay = error_dialog_->IsVisible() ? error_dialog_->Render()
                           : helper_->IsVisible()     ? helper_->Render()
                                                      : ftxui::text("");

  return ftxui::dbox({terminal, overlay});
}

/* ********************************************************************************************** */

bool Terminal::OnEvent(ftxui::Event event) {
  // Treat any pending custom event
  OnCustomEvent();

  // Cannot do anything while dialog box is opened
  if (error_dialog_->IsVisible()) return error_dialog_->OnEvent(event);

  // Or if helper is opened
  if (helper_->IsVisible()) return helper_->OnEvent(event);

  // Global commands
  if (OnGlobalModeEvent(event)) return true;

  // Block commands
  bool result =
      std::any_of(children_.begin(), children_.end(),
                  [&event](const ftxui::Component& child) { return child->OnEvent(event); });

  return result;
}

/* ********************************************************************************************** */

int Terminal::CalculateNumberBars() {
  // In this case, should calculate new size for audio visualizer (number of bars for spectrum)
  int block_width = std::static_pointer_cast<Block>(children_.at(0))->GetSize().width;

  // crazy math function = (a - b - c - d) / e;
  // considering these:
  // a = terminal maximum width
  // b = ListDirectory width
  // c = border characters
  // d = some constant to represent spacing between bars
  // e = bar thickness + 1
  float crazy_math = ((float)(size_.dimx - block_width - 2 - 10) / 4);
  crazy_math += (float)(crazy_math * .01);

  // Round to nearest odd number
  int number_bars;
  if ((int)floor(crazy_math) % 2 == 0)
    number_bars = (int)floor(crazy_math);
  else
    number_bars = (int)ceil(crazy_math);

  return number_bars;
}

/* ********************************************************************************************** */

void Terminal::OnCustomEvent() {
  // Events ignored for logging
  static std::set<CustomEvent::Identifier> ignored{CustomEvent::Identifier::DrawAudioSpectrum,
                                                   CustomEvent::Identifier::Refresh,
                                                   CustomEvent::Identifier::SetFocused};

  while (receiver_->HasPending()) {
    CustomEvent event;
    if (!receiver_->Receive(&event)) break;

    // If it is not an ignored event, log it
    if (ignored.find(event.GetId()) == ignored.end()) LOG("Received a new custom event=", event);

    // As this class centralizes any event sending (to an external notifier or some child block),
    // first gotta check if this event is specifically for the player
    switch (event.type) {
      case CustomEvent::Type::FromInterfaceToAudioThread:
        // If event is handled, skip to next event
        if (HandleEventFromInterfaceToAudioThread(event)) continue;
        break;

      case CustomEvent::Type::FromAudioThreadToInterface:
        // If event is handled, skip to next event
        if (HandleEventFromAudioThreadToInterface(event)) continue;
        break;

      case CustomEvent::Type::FromInterfaceToInterface:
        // If event is handled, skip to next event
        if (HandleEventFromInterfaceToInterface(event)) continue;
        break;
    }

    // Otherwise, send it to children blocks
    for (const auto& child : children_) {
      auto block = std::static_pointer_cast<Block>(child);
      if (block->OnCustomEvent(event)) {
        break;  // Skip to next event
      }
    }
  }
}

/* ********************************************************************************************** */

bool Terminal::OnGlobalModeEvent(const ftxui::Event& event) {
  // Exit application
  if (event == ftxui::Event::Character('q')) {
    LOG("Handle key to exit");
    Exit();

    return true;
  }

  // Show helper
  if (event == ftxui::Event::F1) {
    LOG("Handle key to show helper");
    helper_->Show();

    return true;
  }

  // Switch focus
  if (event == ftxui::Event::Tab) {
    LOG("Handle key to focus next UI block");

    // Send event to focus next UI block
    auto focus_event = interface::CustomEvent::SetNextFocused();
    ProcessEvent(focus_event);

    return true;
  }

  // Switch focus reverse
  if (event == ftxui::Event::TabReverse) {
    LOG("Handle key to focus previous UI block");

    // Send event to focus next/previous UI block
    auto focus_event = interface::CustomEvent::SetPreviousFocused();
    ProcessEvent(focus_event);

    return true;
  }

  // Switch block focus based on a predefined index
  if (HandleEventToSwitchBlockFocus(event)) return true;

  return false;
}

/* ********************************************************************************************** */

bool Terminal::HandleEventToSwitchBlockFocus(const ftxui::Event& event) {
  if (!event.is_character()) return false;

  // Set ListDirectory block as focused (shift+1)
  if (event == ftxui::Event::Character('!')) {
    auto event_focus = interface::CustomEvent::SetFocused(model::BlockIdentifier::ListDirectory);
    ProcessEvent(event_focus);

    return true;
  }

  // Set FileInfo block as focused (shift+2)
  if (event == ftxui::Event::Character('@')) {
    auto event_focus = interface::CustomEvent::SetFocused(model::BlockIdentifier::FileInfo);
    ProcessEvent(event_focus);

    return true;
  }

  // Set TabViewer block as focused (shift+3)
  if (event == ftxui::Event::Character('#')) {
    auto event_focus = interface::CustomEvent::SetFocused(model::BlockIdentifier::TabViewer);
    ProcessEvent(event_focus);

    return true;
  }

  // Set MediaPlayer block as focused (shift+4)
  if (event == ftxui::Event::Character('$')) {
    auto event_focus = interface::CustomEvent::SetFocused(model::BlockIdentifier::MediaPlayer);
    ProcessEvent(event_focus);

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool Terminal::HandleEventFromInterfaceToAudioThread(const CustomEvent& event) {
  bool event_handled = true;

  auto media_ctl = notifier_.lock();
  if (!media_ctl) {
    // TODO: improve handling here and also for each method call
    return !event_handled;
  }

  switch (event.GetId()) {
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
      // Send content directly to audio analysis thread
      media_ctl->ResizeAnalysisOutput(content);

      // Update UI with new size
      auto event_bars =
          interface::CustomEvent::DrawAudioSpectrum(std::vector<double>(content, 0.001));
      ProcessEvent(event_bars);
    } break;

    case CustomEvent::Identifier::SeekForwardPosition: {
      auto content = event.GetContent<int>();
      media_ctl->SeekForwardPosition(content);
    } break;

    case CustomEvent::Identifier::SeekBackwardPosition: {
      auto content = event.GetContent<int>();
      media_ctl->SeekBackwardPosition(content);
    } break;

    case CustomEvent::Identifier::ApplyAudioFilters: {
      auto content = event.GetContent<model::EqualizerPreset>();
      media_ctl->ApplyAudioFilters(content);

    } break;

    default:
      event_handled = false;
      break;
  }

  return event_handled;
}

/* ********************************************************************************************** */

bool Terminal::HandleEventFromAudioThreadToInterface(const CustomEvent&) const {
  // Do nothing here, let Blocks handle it
  return false;
}

/* ********************************************************************************************** */

bool Terminal::HandleEventFromInterfaceToInterface(const CustomEvent& event) {
  bool event_handled = true;

  // To change bar animation shown in audio_visualizer, terminal is necessary to get real block
  // size and calculate maximum number of bars
  switch (event.GetId()) {
    case CustomEvent::Identifier::ChangeBarAnimation: {
      // Recalculate maximum number of bars to show in spectrum visualizer
      int number_bars = CalculateNumberBars();

      // Pass this new value to spectrum visualizer calculate based on the current animation
      auto event_calculate = CustomEvent::CalculateNumberOfBars(number_bars);
      ProcessEvent(event_calculate);
    } break;

    case CustomEvent::Identifier::SetPreviousFocused: {
      // Remove focus from old block
      auto old_focused = std::static_pointer_cast<Block>(children_.at(focused_index_));
      old_focused->SetFocused(false);

      // Calculate new block index to be focused
      focused_index_ = focused_index_ == 0 ? (kMaxBlocks - 1) : focused_index_ - 1;

      // Set focus on new block
      auto new_focused = std::static_pointer_cast<Block>(children_.at(focused_index_));
      new_focused->SetFocused(true);

      LOG("Changed block focus from ", old_focused->GetId(), " to ", new_focused->GetId());
    } break;

    case CustomEvent::Identifier::SetNextFocused: {
      // Remove focus from old block
      auto old_focused = std::static_pointer_cast<Block>(children_.at(focused_index_));
      old_focused->SetFocused(false);

      // Calculate new block index to be focused
      focused_index_ = focused_index_ == 3 ? 0 : focused_index_ + 1;

      // Set focus on new block
      auto new_focused = std::static_pointer_cast<Block>(children_.at(focused_index_));
      new_focused->SetFocused(true);

      LOG("Changed block focus from ", old_focused->GetId(), " to ", new_focused->GetId());
    } break;

    case CustomEvent::Identifier::SetFocused: {
      auto content = event.GetContent<model::BlockIdentifier>();
      int new_index = GetIndexFromBlockIdentifier(content);

      // Do nothing
      if (focused_index_ == new_index) break;

      // Remove focus from old block
      auto old_focused = std::static_pointer_cast<Block>(children_.at(focused_index_));
      old_focused->SetFocused(false);

      // Update new block index to be focused
      focused_index_ = new_index;

      // Set focus on new block
      auto new_focused = std::static_pointer_cast<Block>(children_.at(focused_index_));
      new_focused->SetFocused(true);

      LOG("Changed block focus from ", old_focused->GetId(), " to ", new_focused->GetId());
    } break;

    case CustomEvent::Identifier::ShowHelper: {
      helper_->Show();
    } break;

    case CustomEvent::Identifier::Exit: {
      Exit();
    } break;

    default:
      event_handled = false;
      break;
  }

  return event_handled;
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
  // Get error message
  std::string message{error::ApplicationError::GetMessage(id)};

  // Log error and show it on dialog
  ERROR(message);
  error_dialog_->SetErrorMessage(message);

  last_error_ = id;
}

/* ********************************************************************************************** */

int Terminal::GetIndexFromBlockIdentifier(const model::BlockIdentifier& id) const {
  switch (id) {
    case model::BlockIdentifier::ListDirectory:
      return 0;
    case model::BlockIdentifier::FileInfo:
      return 1;
    case model::BlockIdentifier::TabViewer:
      return 2;
    case model::BlockIdentifier::MediaPlayer:
      return 3;
    default:
      return 0;
  }
}

}  // namespace interface
