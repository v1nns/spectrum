#include "view/base/terminal.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <memory>
#include <set>
#include <string>
#include <utility>

#ifndef SPECTRUM_DEBUG
#include "audio/driver/ffmpeg.h"
#include "web/driver/ytdlp_wrapper.h"
#else
#include "debug/dummy_decoder.h"
#include "debug/dummy_stream_fetcher.h"
#endif

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/terminal.hpp"
#include "model/application_error.h"
#include "model/bar_animation.h"
#include "model/block_identifier.h"
#include "model/playlist_operation.h"
#include "util/file_handler.h"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/block.h"
#include "view/base/keybinding.h"
#include "view/block/file_info.h"
#include "view/block/main_content.h"
#include "view/block/media_player.h"
#include "view/block/sidebar.h"
#include "view/element/style.h"

namespace interface {

//! To make life easier
bool operator!=(const ftxui::Dimensions& lhs, const ftxui::Dimensions& rhs) {
  return std::tie(lhs.dimx, lhs.dimy) != std::tie(rhs.dimx, rhs.dimy);
}

/* ********************************************************************************************** */

std::shared_ptr<Terminal> Terminal::Create(const std::string& initial_path,
                                           const std::shared_ptr<util::FileHandler>& file_handler) {
  LOG("Create new instance of terminal");

  // Simply extend the Terminal class, as we do not want to expose the default constructor, neither
  // do we want to use std::make_shared explicitly calling operator new()
  struct MakeSharedEnabler : public Terminal {};
  auto terminal = std::make_shared<MakeSharedEnabler>();

  // Initialize internal components
  terminal->Init(initial_path, file_handler);

  return terminal;
}

/* ********************************************************************************************** */

void Terminal::Init(const std::string& initial_path,
                    const std::shared_ptr<util::FileHandler>& file_handler) {
  LOG("Initialize terminal");

  // As this terminal will hold all these interface blocks, there is nothing better than
  // use itself as a mediator to send events between them
  std::shared_ptr<EventDispatcher> dispatcher = shared_from_this();

  // Callback to check if file contains an audio stream, used before trying to play a file
  const std::function<bool(const util::File&)> contains_audio_cb =
#ifndef SPECTRUM_DEBUG
      driver::FFmpeg::ContainsAudioStream;
#else
      driver::DummyDecoder::ContainsAudioStream;
#endif

  // Callback to check if songs can be added from URL (it depends on an external program)
  const std::function<bool()> stream_available_cb =
#ifndef SPECTRUM_DEBUG
      driver::YtDlpWrapper::IsAvailable;
#else
      driver::DummyStreamFetcher::IsAvailable;
#endif

  // Callback to import songs from playlist URL (it also depends on an external program)
  const PlaylistDialog::PlaylistFetchCallback fetch_playlist_cb =
#ifndef SPECTRUM_DEBUG
      driver::YtDlpWrapper::ExtractPlaylist;
#else
      driver::DummyStreamFetcher::ExtractPlaylist;
#endif

  // Create blocks
  auto sidebar =
      std::make_shared<Sidebar>(dispatcher, initial_path, file_handler, contains_audio_cb);
  auto file_info = std::make_shared<FileInfo>(dispatcher);
  auto tab_viewer = std::make_shared<MainContent>(dispatcher, file_handler);
  auto media_player = std::make_shared<MediaPlayer>(dispatcher, file_handler);

  // As default, make Sidebar focused to receive input commands
  sidebar->SetFocused(true);

  // Make every block as a child of this terminal
  // WARNING: be careful with the order you add (must be synced with unique indexes in header)
  Add(sidebar);
  Add(file_info);
  Add(tab_viewer);
  Add(media_player);

  // Create dialogs
  error_dialog_ = std::make_unique<ErrorDialog>(dispatcher);
  help_dialog_ = std::make_unique<HelpDialog>(dispatcher);
  playlist_dialog_ = std::make_unique<PlaylistDialog>(dispatcher, contains_audio_cb, initial_path,
                                                      stream_available_cb, fetch_playlist_cb);
  question_dialog_ = std::make_unique<QuestionDialog>(dispatcher);

  // Pickers save what is chosen in settings
  auto settings_handler =
      file_handler != nullptr ? file_handler : std::make_shared<util::FileHandler>();

  // Create theme picker, which also restores theme chosen on last run
  theme_picker_ = std::make_unique<ThemePicker>(settings_handler);

  // Create device picker, to choose audio output device
  device_picker_ = std::make_unique<DevicePicker>(dispatcher, settings_handler);
}

/* ********************************************************************************************** */

void Terminal::Exit() const {
  INFO("Exit from terminal");

  // Trigger exit callback
  if (cb_exit_) cb_exit_();
}

/* ********************************************************************************************** */

void Terminal::RegisterPlayerNotifier(const std::shared_ptr<audio::Notifier>& notifier) {
  notifier_ = notifier;
  notifier_registered_ = true;

  // Now that audio thread can be reached, handle events sent to it before
  for (const auto& event : std::exchange(pending_audio_events_, {})) {
    LOG("Handle event sent to audio thread before it was registered, event=", event);
    HandleEventFromInterfaceToAudioThread(event);
  }
}

/* ********************************************************************************************** */

void Terminal::RegisterEventSenderCallback(EventCallback cb) {
  cb_send_event_ = cb;

  // Force a refresh to handle any pending custom event
  // (this is necessary, in order to update UI with volume information)
  cb_send_event_(ftxui::Event::Custom);
}

/* ********************************************************************************************** */

void Terminal::RegisterExitCallback(Callback cb) { cb_exit_ = cb; }

/* ********************************************************************************************** */

ftxui::Element Terminal::Render() {
  if (children_.empty() || children_.size() != 4) {
    ERROR("Terminal is empty, it has no child block");
    Exit();
  }

  // Check if terminal has been resized
  if (auto current_size = cb_size_(); size_ != current_size) {
    LOG("Resize terminal with new value={x:", current_size.dimx, " y:", current_size.dimy, "}");
    size_ = current_size;

    // Recalculate maximum number of bars to show in spectrum graphic
    int number_bars = CalculateNumberBars();

    // Send value to spectrum visualizer
    auto event_calculate = CustomEvent::CalculateNumberOfBars(number_bars);
    SendEvent(event_calculate);
  }

  // Blocks would be cut or overlapped, so ask user to resize terminal instead
  // Colors for anything without a color of its own (when theme does not use the ones from terminal)
  const auto& colors = GetTheme().screen;
  const auto screen = ftxui::color(colors.foreground) | ftxui::bgcolor(colors.background);

  if (IsTooSmall()) {
    return RenderTooSmall() | screen;
  }

  ftxui::Element terminal;

  if (!fullscreen_mode_) {
    // Render each block
    ftxui::Element sidebar = children_.at(kBlockSidebar)->Render();
    ftxui::Element file_info = children_.at(kBlockFileInfo)->Render();
    ftxui::Element tab_viewer = children_.at(kBlockMainContent)->Render();
    ftxui::Element media_player = children_.at(kBlockMediaPlayer)->Render();

    // Glue everything together
    terminal = ftxui::hbox({
        ftxui::vbox({sidebar, file_info}),
        ftxui::vbox({tab_viewer, media_player}) | ftxui::xflex_grow,
    });
  } else {
    // Render only spectrum visualizer
    auto tab_viewer = std::static_pointer_cast<MainContent>(children_.at(kBlockMainContent));
    terminal = tab_viewer->RenderFullscreen() | ftxui::xflex_grow;
  }

  // Apply decorator to dim terminal
  ftxui::Decorator dim = IsDialogVisible() ? ftxui::dim : ftxui::nothing;

  // Render element as overlay
  ftxui::Element overlay = GetOverlay();

  return ftxui::dbox({terminal | dim, overlay}) | screen;
}

/* ********************************************************************************************** */

ftxui::Element Terminal::RenderTooSmall() const {
  auto size_to_string = [](int columns, int lines) {
    return std::to_string(columns) + "x" + std::to_string(lines);
  };

  const std::string exit_key = util::EventToString(keybinding::General::ExitApplication);

  return ftxui::vbox({
             ftxui::text("Terminal too small") | ftxui::bold | ftxui::hcenter,
             ftxui::text(""),
             ftxui::text("Current: " + size_to_string(size_.dimx, size_.dimy)) | ftxui::hcenter,
             ftxui::text("Minimum: " + size_to_string(kMinColumns, kMinLines)) | ftxui::hcenter,
             ftxui::text(""),
             ftxui::text("Resize it or press " + exit_key + " to quit") | ftxui::dim |
                 ftxui::hcenter,
         }) |
         ftxui::center | ftxui::flex;
}

/* ********************************************************************************************** */

bool Terminal::OnEvent(ftxui::Event event) {
  // Treat any pending custom event
  OnCustomEvent();

  // Translate terminal-specific key sequences (e.g. Home/End under tmux)
  event = keybinding::Normalize(event);

  // Blocks are not visible, so do not let user interact with them (only quit is allowed)
  if (IsTooSmall()) {
    if (event == keybinding::General::ExitApplication) {
      LOG("Handle key to exit (terminal too small)");
      Exit();
      return true;
    }

    return false;
  }

  // Cannot do anything while dialog box is opened
  if (error_dialog_->IsVisible()) return error_dialog_->OnEvent(event);

  // Or if helper is opened
  if (help_dialog_->IsVisible()) return help_dialog_->OnEvent(event);

  // Or if playlist manager is opened
  if (playlist_dialog_->IsVisible()) return playlist_dialog_->OnEvent(event);

  // Or if question dialog is opened
  if (question_dialog_->IsVisible()) return question_dialog_->OnEvent(event);

  // Or if theme picker is opened
  if (theme_picker_->IsVisible()) return theme_picker_->OnEvent(event);

  // Or if device picker is opened
  if (device_picker_->IsVisible()) return device_picker_->OnEvent(event);

  // Animation picker is shown over spectrum visualizer without blocking anything else, but a
  // click anywhere outside of it closes it (so it must handle mouse before any block)
  if (event.is_mouse() &&
      std::static_pointer_cast<MainContent>(children_.at(kBlockMainContent))
          ->OnPickerMouseEvent(event)) {
    return true;
  }

  // Global commands
  if (global_mode_ && OnGlobalModeEvent(event)) return true;

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

int Terminal::CalculateNumberBars(const std::optional<model::BarAnimation>& animation) {
  static model::BarAnimation last_animation = model::BarAnimation::LAST;
  if (animation.has_value()) last_animation = *animation;

  // Width available for spectrum visualizer: in fullscreen mode it is the whole terminal,
  // otherwise it is what remains after sidebar block (with its border) and visualizer border
  int available = size_.dimx;

  if (!fullscreen_mode_) {
    const int sidebar_width =
        std::static_pointer_cast<Block>(children_.at(kBlockSidebar))->GetSize().width;

    // Both sidebar and visualizer have borders on left and right sides
    available -= sidebar_width + (2 * kBorderSize) + (2 * kBorderSize);
  }

  const int bar_width =
      std::static_pointer_cast<MainContent>(children_.at(kBlockMainContent))->GetBarWidth();
  const int bar_spacing = model::IsAnimationSpaced(last_animation) ? 1 : 0;

  // Maximum number of bars that fit (N bars need N * width + (N - 1) * spacing columns)
  int number_bars = (available + bar_spacing) / (bar_width + bar_spacing);

  // Bars are split between both audio channels, so round it down to an even number
  number_bars -= number_bars % 2;

  return std::max(number_bars, 0);
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
  if (event == keybinding::General::ExitApplication) {
    LOG("Handle key to exit");
    Exit();

    return true;
  }

  // Show helper
  if (event == keybinding::General::ShowHelper) {
    LOG("Handle key to show helper");
    help_dialog_->Show();

    return true;
  }

  // Show theme picker
  if (event == keybinding::General::ChangeTheme) {
    LOG("Handle key to show theme picker");
    theme_picker_->Open();

    return true;
  }

  // Show audio output device picker (nothing to choose from while audio thread cannot be reached)
  if (event == keybinding::General::ChangeAudioDevice) {
    LOG("Handle key to show audio output device picker");
    if (auto media_ctl = notifier_.lock(); media_ctl) {
      device_picker_->Open(media_ctl->GetAudioDevices());
    }

    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool Terminal::OnFullscreenModeEvent(const ftxui::Event& event) {
  // In this case, only spectrum visualizer and media player may handle this event
  std::vector<ftxui::Component>::const_iterator first = children_.begin() + kBlockMainContent;
  std::vector<ftxui::Component>::const_iterator last = children_.end();

  // Media player is not rendered, so it must not handle mouse events (only its keybindings)
  if (event.is_mouse()) last = first + 1;

  if (bool event_handled = std::any_of(
          first, last, [&event](const ftxui::Component& child) { return child->OnEvent(event); });
      event_handled)
    return true;

  return false;
}

/* ********************************************************************************************** */

bool Terminal::OnFocusEvent(const ftxui::Event& event) {
  // Switch focus
  if (event == keybinding::Navigation::Tab) {
    LOG("Handle key to focus next UI block");
    return HandleEventFromInterfaceToInterface(interface::CustomEvent::SetNextFocused());
  }

  // Switch focus reverse
  if (event == keybinding::Navigation::TabReverse) {
    LOG("Handle key to focus previous UI block");
    return HandleEventFromInterfaceToInterface(interface::CustomEvent::SetPreviousFocused());
  }

  // To avoid checking the upcoming if-statements, first check if event is a character
  if (!event.is_character()) return false;

  // Set Sidebar block as focused
  if (event == keybinding::General::FocusSidebar) {
    LOG("Handle key to focus Sidebar block");
    UpdateFocus(focused_index_, kBlockSidebar);
    return true;
  }

  // Set FileInfo block as focused
  if (event == keybinding::General::FocusInfo) {
    LOG("Handle key to focus FileInfo block");
    UpdateFocus(focused_index_, kBlockFileInfo);
    return true;
  }

  // Set MainContent block as focused
  if (event == keybinding::General::FocusMainContent) {
    LOG("Handle key to focus MainContent block");
    UpdateFocus(focused_index_, kBlockMainContent);
    return true;
  }

  // Set MediaPlayer block as focused
  if (event == keybinding::General::FocusPlayer) {
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

  // Application is still starting, so keep it until audio thread can be reached
  if (!media_ctl && !notifier_registered_) {
    LOG("Media controller not registered yet, keep event to audio thread=", event);
    pending_audio_events_.push_back(event);
    return event_handled;
  }

  if (!media_ctl) {
    // This happens while application is exiting, so there is no audio thread to handle it anymore
    WARN("Cannot lock media controller, event to audio thread will be discarded, event=", event);
    return !event_handled;
  }

  switch (event.GetId()) {
    case CustomEvent::Identifier::NotifyFileSelection: {
      auto content = event.GetContent<std::filesystem::path>();
      media_ctl->NotifyFileSelection(content);
    } break;

    case CustomEvent::Identifier::PauseSong:
      media_ctl->Pause();
      break;

    case CustomEvent::Identifier::ResumeSong: {
      auto run_animation = event.GetContent<bool>();
      media_ctl->Resume(run_animation);
    } break;

    case CustomEvent::Identifier::StopSong:
      media_ctl->Stop();
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

    case CustomEvent::Identifier::NotifyPlaylistSelection: {
      auto content = event.GetContent<model::Playlist>();
      media_ctl->NotifyPlaylistSelection(content);
    } break;

    case CustomEvent::Identifier::NotifyDialogClosed: {
      media_ctl->NotifyErrorDialogClosed();
    } break;

    case CustomEvent::Identifier::SkipToNextPlaylistSong: {
      media_ctl->SkipToNextSong();
    } break;

    case CustomEvent::Identifier::SkipToPreviousPlaylistSong: {
      media_ctl->SkipToPreviousSong();
    } break;

    case CustomEvent::Identifier::SetRepeatMode: {
      media_ctl->SetRepeatMode(event.GetContent<model::RepeatMode>());
    } break;

    case CustomEvent::Identifier::SetShuffle: {
      media_ctl->SetShuffle(event.GetContent<bool>());
    } break;

    case CustomEvent::Identifier::SetAudioDevice: {
      media_ctl->SetAudioDevice(event.GetContent<std::string>());
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
    case CustomEvent::Identifier::DisableGlobalEvent:
    case CustomEvent::Identifier::EnableGlobalEvent: {
      global_mode_ = event.GetId() == CustomEvent::Identifier::EnableGlobalEvent ? true : false;
    } break;

    case CustomEvent::Identifier::ChangeBarAnimation:
    case CustomEvent::Identifier::UpdateBarWidth: {
      std::optional<model::BarAnimation> animation;
      if (event.GetId() == CustomEvent::Identifier::ChangeBarAnimation)
        animation = event.GetContent<model::BarAnimation>();

      // Recalculate maximum number of bars to show in spectrum visualizer
      int number_bars = CalculateNumberBars(animation);

      // Pass this new value to spectrum visualizer calculate based on the current animation
      auto event_calculate = CustomEvent::CalculateNumberOfBars(number_bars);
      ProcessEvent(event_calculate);
    } break;

    case CustomEvent::Identifier::SetPreviousFocused: {
      // Calculate new block index to be focused
      int new_index = focused_index_ == kBlockSidebar || focused_index_ == kInvalidIndex
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
      const auto& content = event.GetContent<model::BlockIdentifier>();
      int new_index = GetIndexFromBlockIdentifier(content);

      UpdateFocus(focused_index_, new_index);
    } break;

    case CustomEvent::Identifier::ShowHelper: {
      help_dialog_->Show();
    } break;

    case CustomEvent::Identifier::ToggleFullscreen: {
      fullscreen_mode_ = !fullscreen_mode_;

      // Recalculate maximum number of bars to show in spectrum graphic
      int number_bars = CalculateNumberBars();

      // Send value to spectrum visualizer
      auto event_calculate = CustomEvent::CalculateNumberOfBars(number_bars);
      SendEvent(event_calculate);
    } break;

    case CustomEvent::Identifier::ShowPlaylistManager: {
      const auto& content = event.GetContent<model::PlaylistOperation>();
      playlist_dialog_->Open(content);
    } break;

    case CustomEvent::Identifier::ShowQuestionDialog: {
      const auto& content = event.GetContent<model::QuestionData>();
      question_dialog_->SetMessage(content);
      question_dialog_->Open();
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

  // Callback is registered only after UI is created, so any event sent before it (e.g. warning
  // while listing initial directory) is handled when callback gets registered
  if (cb_send_event_) cb_send_event_(ftxui::Event::Custom);  // force a refresh
}

/* ********************************************************************************************** */

void Terminal::ProcessEvent(const CustomEvent& event) {
  // This method was planned to execute any custom event while Screen loop is not running yet
  sender_->Send(event);
  OnCustomEvent();
}

/* ********************************************************************************************** */

void Terminal::SetApplicationError(error::Code id, const std::string& detail) {
  // Get error message
  std::string message{error::ApplicationError::GetMessage(id)};

  last_error_ = id;

  // Warning is shown briefly by media player, without interrupting user
  if (error::ApplicationError::GetLevel(id) == error::Level::Warning) {
    WARN(message, " detail=", std::quoted(detail));
    SendEvent(CustomEvent::ShowWarning(detail.empty() ? message : message + ": " + detail));
    return;
  }

  ERROR(message, " detail=", std::quoted(detail));
  error_dialog_->SetErrorMessage(message, detail);

  // Error may come from another thread while nothing else is asking UI to be rendered (e.g. no
  // song is playing), so force a refresh to show dialog right away
  SendEvent(CustomEvent::Refresh());
}

/* ********************************************************************************************** */

constexpr int Terminal::GetIndexFromBlockIdentifier(const model::BlockIdentifier& id) const {
  switch (id) {
    case model::BlockIdentifier::Sidebar:
      return kBlockSidebar;
    case model::BlockIdentifier::FileInfo:
      return kBlockFileInfo;
    case model::BlockIdentifier::MainContent:
      return kBlockMainContent;
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

  model::BlockIdentifier old_block = model::BlockIdentifier::None;
  model::BlockIdentifier new_block = model::BlockIdentifier::None;

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

/* ********************************************************************************************** */

ftxui::Element Terminal::GetOverlay() const {
  if (error_dialog_->IsVisible()) return error_dialog_->Render(size_);

  if (help_dialog_->IsVisible()) return help_dialog_->Render(size_);

  if (playlist_dialog_->IsVisible()) return playlist_dialog_->Render(size_);

  if (question_dialog_->IsVisible()) return question_dialog_->Render(size_);

  if (theme_picker_->IsVisible()) return theme_picker_->Render() | ftxui::center;

  if (device_picker_->IsVisible()) return device_picker_->Render() | ftxui::center;

  return ftxui::emptyElement();
}

}  // namespace interface
