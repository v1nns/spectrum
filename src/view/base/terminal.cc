
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
#include "model/block_identifier.h"
#include "util/logger.h"
#include "view/base/block.h"
#include "view/block/file_info.h"
#include "view/block/media_player.h"
#include "view/block/sidebar_content/list_directory.h"
#include "view/block/main_tab.h"

namespace interface {

//! To make life easier
bool operator!=(const ftxui::Dimensions& lhs, const ftxui::Dimensions& rhs) {
  return std::tie(lhs.dimx, lhs.dimy) != std::tie(rhs.dimx, rhs.dimy);
}

/* ********************************************************************************************** */

std::shared_ptr<Terminal> Terminal::Create(const std::string& initial_path) {
  LOG("Create new instance of terminal");

  // Simply extend the Terminal class, as we do not want to expose the default constructor, neither
  // do we want to use std::make_shared explicitly calling operator new()
  struct MakeSharedEnabler : public Terminal {};
  auto terminal = std::make_shared<MakeSharedEnabler>();

  // Initialize internal components
  terminal->Init(initial_path);

  return terminal;
}

/* ********************************************************************************************** */

void Terminal::Init(const std::string& initial_path) {
  LOG("Initialize terminal");

  // As this terminal will hold all these interface blocks, there is nothing better than
  // use itself as a mediator to send events between them
  std::shared_ptr<EventDispatcher> dispatcher = shared_from_this();

  // Create blocks
  auto list_dir = std::make_shared<ListDirectory>(dispatcher, initial_path);
  auto file_info = std::make_shared<FileInfo>(dispatcher);
  auto tab_viewer = std::make_shared<MainTab>(dispatcher);
  auto media_player = std::make_shared<MediaPlayer>(dispatcher);

  // As default, make ListDirectory focused to receive input commands
  list_dir->SetFocused(true);

  // Make every block as a child of this terminal
  // WARNING: be careful with the order you add (must be synced with unique indexes in header)
  Add(list_dir);
  Add(file_info);
  Add(tab_viewer);
  Add(media_player);
}

/* ********************************************************************************************** */

void Terminal::Exit() const {
  LOG("Exit from terminal");

  // Trigger exit callback
  if (cb_exit_) cb_exit_();
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

  ftxui::Element terminal;

  if (!fullscreen_mode_) {
    // Render each block
    ftxui::Element list_dir = children_.at(kBlockListDirectory)->Render();
    ftxui::Element file_info = children_.at(kBlockFileInfo)->Render();
    ftxui::Element tab_viewer = children_.at(kBlockMainTab)->Render();
    ftxui::Element media_player = children_.at(kBlockMediaPlayer)->Render();

    // Glue everything together
    terminal = ftxui::hbox({
        ftxui::vbox({list_dir, file_info}),
        ftxui::vbox({tab_viewer, media_player}) | ftxui::xflex_grow,
    });
  } else {
    // Render only spectrum visualizer
    auto tab_viewer = std::static_pointer_cast<MainTab>(children_.at(kBlockMainTab));
    terminal = tab_viewer->RenderFullscreen() | ftxui::xflex_grow;
  }

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

  // If fullscreen mode is enabled, only a subset of blocks are able to handle this event
  if (fullscreen_mode_) return OnFullscreenModeEvent(event);

  // Block commands
  if (bool event_handled =
          std::any_of(children_.begin(), children_.end(),
                      [&event](const ftxui::Component& child) { return child->OnEvent(event); });
      event_handled)
    return true;

  // Switch block focus based on a predefined index
  if (OnFocusEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

int Terminal::CalculateNumberBars() {
  // In this case, should calculate new size for audio visualizer (number of bars for spectrum)
  auto block_width = static_cast<float>(
      !fullscreen_mode_
          ? std::static_pointer_cast<Block>(children_.at(kBlockListDirectory))->GetSize().width
          : 0);

  auto bar_width = static_cast<float>(
      std::static_pointer_cast<MainTab>(children_.at(kBlockMainTab))->GetBarWidth());

  // crazy math function = (a - b - c - d) / e;
  // considering these:
  // a = terminal maximum width
  // b = ListDirectory width
  // c = border width
  // d = audio bar spacing
  // e = bar width + audio bar spacing
  float border_width = !fullscreen_mode_ ? 2 : -1;
  float crazy_math =
      (static_cast<float>(size_.dimx) - block_width - border_width - 1) / (bar_width + 1);

  // Round to nearest odd number
  crazy_math = static_cast<int>(floor(crazy_math)) % 2 ? crazy_math - 1 : crazy_math;

  // Return number of audio bars
  return static_cast<int>(crazy_math);
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

  // Show general helper
  if (event == ftxui::Event::F1) {
    LOG("Handle key to show general helper");
    helper_->ShowGeneralInfo();

    return true;
  }

  // Show tab helper
  if (event == ftxui::Event::F2) {
    LOG("Handle key to show tab helper");
    helper_->ShowTabInfo();

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool Terminal::OnFullscreenModeEvent(const ftxui::Event& event) {
  // In this case, only spectrum visualizer and media player may handle this event
  std::vector<ftxui::Component>::const_iterator first = children_.begin() + 2;
  std::vector<ftxui::Component>::const_iterator last = children_.end();

  if (bool event_handled = std::any_of(
          first, last, [&event](const ftxui::Component& child) { return child->OnEvent(event); });
      event_handled)
    return true;

  return false;
}

/* ********************************************************************************************** */

bool Terminal::OnFocusEvent(const ftxui::Event& event) {
  // Switch focus
  if (event == ftxui::Event::Tab) {
    LOG("Handle key to focus next UI block");
    return HandleEventFromInterfaceToInterface(interface::CustomEvent::SetNextFocused());
  }

  // Switch focus reverse
  if (event == ftxui::Event::TabReverse) {
    LOG("Handle key to focus previous UI block");
    return HandleEventFromInterfaceToInterface(interface::CustomEvent::SetPreviousFocused());
  }

  // Remove focus from all blocks
  if (focused_index_ != kInvalidIndex && event == ftxui::Event::Escape) {
    LOG("Handle key to remove focus from all blocks");
    UpdateFocus(focused_index_, kInvalidIndex);
    return true;
  }

  // To avoid checking the upcoming if statements, first check if event is a character
  if (!event.is_character()) return false;

  // Set ListDirectory block as focused (shift+1)
  if (event == ftxui::Event::Character('!')) {
    LOG("Handle key to focus ListDirectory block");
    UpdateFocus(focused_index_, kBlockListDirectory);
    return true;
  }

  // Set FileInfo block as focused (shift+2)
  if (event == ftxui::Event::Character('@')) {
    LOG("Handle key to focus FileInfo block");
    UpdateFocus(focused_index_, kBlockFileInfo);
    return true;
  }

  // Set MainTab block as focused (shift+3)
  if (event == ftxui::Event::Character('#')) {
    LOG("Handle key to focus MainTab block");
    UpdateFocus(focused_index_, kBlockMainTab);
    return true;
  }

  // Set MediaPlayer block as focused (shift+4)
  if (event == ftxui::Event::Character('$')) {
    LOG("Handle key to focus MediaPlayer block");
    UpdateFocus(focused_index_, kBlockMediaPlayer);
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
    case CustomEvent::Identifier::ChangeBarAnimation:
    case CustomEvent::Identifier::UpdateBarWidth: {
      // Recalculate maximum number of bars to show in spectrum visualizer
      int number_bars = CalculateNumberBars();

      // Pass this new value to spectrum visualizer calculate based on the current animation
      auto event_calculate = CustomEvent::CalculateNumberOfBars(number_bars);
      ProcessEvent(event_calculate);
    } break;

    case CustomEvent::Identifier::SetPreviousFocused: {
      // Calculate new block index to be focused
      int new_index = focused_index_ == kBlockListDirectory || focused_index_ == kInvalidIndex
                          ? (kMaxBlocks - 1)
                          : focused_index_ - 1;

      UpdateFocus(focused_index_, new_index);
    } break;

    case CustomEvent::Identifier::SetNextFocused: {
      // Calculate new block index to be focused
      int new_index = focused_index_ == kBlockMediaPlayer ? 0 : focused_index_ + 1;

      UpdateFocus(focused_index_, new_index);
    } break;

    case CustomEvent::Identifier::SetFocused: {
      auto content = event.GetContent<model::BlockIdentifier>();
      int new_index = GetIndexFromBlockIdentifier(content);

      UpdateFocus(focused_index_, new_index);
    } break;

    case CustomEvent::Identifier::ShowHelper: {
      helper_->ShowGeneralInfo();
    } break;

    case CustomEvent::Identifier::ToggleFullscreen: {
      fullscreen_mode_ = !fullscreen_mode_;

      // Recalculate maximum number of bars to show in spectrum graphic
      int number_bars = CalculateNumberBars();

      // Send value to spectrum visualizer
      auto event_calculate = CustomEvent::CalculateNumberOfBars(number_bars);
      SendEvent(event_calculate);
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

constexpr int Terminal::GetIndexFromBlockIdentifier(const model::BlockIdentifier& id) const {
  switch (id) {
    case model::BlockIdentifier::ListDirectory:
      return kBlockListDirectory;
    case model::BlockIdentifier::FileInfo:
      return kBlockFileInfo;
    case model::BlockIdentifier::MainTab:
      return kBlockMainTab;
    case model::BlockIdentifier::MediaPlayer:
      return kBlockMediaPlayer;
    default:
      ERROR("Received wrong block identifier=", id);
      return 0;
  }
}

/* ********************************************************************************************** */

void Terminal::UpdateFocus(int old_index, int new_index) {
  // If equal, do nothing
  if (old_index == new_index) return;

  auto old_block = model::BlockIdentifier::None;
  auto new_block = model::BlockIdentifier::None;

  // Remove focus from old block
  if (old_index != kInvalidIndex) {
    auto block = std::static_pointer_cast<Block>(children_.at(old_index));
    block->SetFocused(false);
    old_block = block->GetId();
  }

  // Set focus on newly-focused block
  if (new_index != kInvalidIndex) {
    auto block = std::static_pointer_cast<Block>(children_.at(new_index));
    block->SetFocused(true);
    new_block = block->GetId();
  }

  // Update internal index
  focused_index_ = new_index;

  LOG("Changed block focus from ", old_block, " to ", new_block);
}

}  // namespace interface
