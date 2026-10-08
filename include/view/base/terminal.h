/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_VIEW_BASE_TERMINAL_H_
#define INCLUDE_VIEW_BASE_TERMINAL_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/receiver.hpp"
#include "middleware/media_controller.h"
#include "model/application_error.h"
#include "model/block_identifier.h"
#include "util/file_handler.h"
#include "view/base/block.h"
#include "view/base/custom_event.h"
#include "view/base/event_dispatcher.h"
#include "view/element/device_picker.h"
#include "view/element/error_dialog.h"
#include "view/element/help_dialog.h"
#include "view/element/playlist_dialog.h"
#include "view/element/question_dialog.h"
#include "view/element/theme_picker.h"

#ifdef ENABLE_TESTS
namespace {
class TerminalTest;
}
#endif

//! Forward declaration
namespace audio {
class AudioControl;
}

namespace interface {

//! Using-declaration for every possible callback function
using EventCallback = std::function<void(ftxui::Event)>;
using Callback = std::function<void()>;

/**
 * @brief Manages the whole screen and contains all block views
 */
class Terminal : public EventDispatcher, public ftxui::ComponentBase {
  static constexpr int kMaxBlocks = 4;      //!< Maximum number of blocks (used for focus control)
  static constexpr int kInvalidIndex = -1;  //!< Default index to remove focus from blocks

  //! Unique index for each block rendered by terminal class
  //! WARNING: focus handling will obey this block order
  static constexpr int kBlockSidebar = 0;
  static constexpr int kBlockFileInfo = 1;
  static constexpr int kBlockMainContent = 2;
  static constexpr int kBlockMediaPlayer = 3;

  static constexpr int kBorderSize = 1;  //!< Columns used by a block border (on each side)

  //! Minimum terminal size to render all blocks without cutting their content: width is limited
  //! by media player (buttons + volume) next to sidebar, and height leaves room for a few files in
  //! sidebar besides the fixed height from file information and media player blocks
  static constexpr int kMinColumns = 105;
  static constexpr int kMinLines = 24;

  /**
   * @brief Construct a new Terminal object
   */
  Terminal() = default;

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return Terminal object
   * @param initial_path Initial path to list files (optional)
   * @param file_handler Utility handler to load/save playlists and settings (optional, a default
   * one is used when it is null)
   * @return std::shared_ptr<Terminal> Terminal instance
   */
  static std::shared_ptr<Terminal> Create(
      const std::string& initial_path,
      const std::shared_ptr<util::FileHandler>& file_handler = nullptr);

  /**
   * @brief Destroy the Terminal object. Base class will do the rest (release resources by detaching
   * all blocks, a.k.a. children)
   */
  ~Terminal() override = default;

  //! Remove these
  Terminal(const Terminal& other) = delete;             // copy constructor
  Terminal(Terminal&& other) = delete;                  // move constructor
  Terminal& operator=(const Terminal& other) = delete;  // copy assignment
  Terminal& operator=(Terminal&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  /**
   * @brief Initialize internal components for Terminal object
   * @param initial_path Initial path to list files (optional)
   * @param file_handler Utility handler to load/save playlists and settings (optional)
   */
  void Init(const std::string& initial_path,
            const std::shared_ptr<util::FileHandler>& file_handler);

  /**
   * @brief Force application to exit
   */
  void Exit() const;

  /* ******************************************************************************************** */
  //! Binds and registrations
 public:
  /**
   * @brief Register notifier to send events from interface to player
   * @param notifier Player notifier
   */
  void RegisterPlayerNotifier(const std::shared_ptr<audio::Notifier>& notifier);

  /**
   * @brief Bind an external send event function to an internal function
   * @param cb Callback function to send custom events to terminal user interface
   */
  void RegisterEventSenderCallback(EventCallback cb);

  /**
   * @brief Bind an external exit function to an internal function
   * @param cb Callback function to exit graphical application
   */
  void RegisterExitCallback(Callback cb);

  /* ******************************************************************************************** */
  //! UI Interface API

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event) override;

  /**
   * @brief Based on maximum terminal size, calculate how many bars can be shown in spectrum window
   * @param animation Current animation identifier (optional)
   * @return Number of bars
   */
  int CalculateNumberBars(const std::optional<model::BarAnimation>& animation = std::nullopt);

  /* ******************************************************************************************** */
  //! Internal event handling
 private:
  /**
   * @brief Handles any pending custom event, to broadcast it to children or audio thread
   */
  void OnCustomEvent();

  /**
   * @brief Handle event not associated with any UI block
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnGlobalModeEvent(const ftxui::Event& event);

  /**
   * @brief Get help section related to what is focused (block and its active tab)
   * @return Help section
   */
  HelpDialog::Section GetHelpSection() const;

  /**
   * @brief Handle event when fullscreen mode is enabled
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnFullscreenModeEvent(const ftxui::Event& event);

  /**
   * @brief Handle event to switch block focus
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnFocusEvent(const ftxui::Event& event);

  /**
   * @brief Handle custom events sent from interface to audio thread (music player)
   * @param event Received custom event
   * @return true if event was handled, otherwise false
   */
  bool HandleEventFromInterfaceToAudioThread(const CustomEvent& event);

  /**
   * @brief Handle custom events sent from audio thread (music player) to interface
   * @param event Received custom event
   * @return true if event was handled, otherwise false
   */
  bool HandleEventFromAudioThreadToInterface(const CustomEvent& event) const;

  /**
   * @brief Handle custom events sent from interface to interface
   * @param event Received custom event
   * @return true if event was handled, otherwise false
   */
  bool HandleEventFromInterfaceToInterface(const CustomEvent& event);

  /* ******************************************************************************************** */
  //! UI Event dispatching and Interface
 public:
  //! Send event to blocks
  void SendEvent(const CustomEvent& event) override;

  //! Add a custom event to internal queue and process right away
  void ProcessEvent(const CustomEvent& event) override;

  //! Set application error (can be originated from controller or any interface::block)
  void SetApplicationError(error::Code id, const std::string& detail) override;

  /* ******************************************************************************************** */
  //! Utils
 private:
  //! Get internal block index based on block identifier
  constexpr int GetIndexFromBlockIdentifier(const model::BlockIdentifier& id) const;

  /**
   * @brief Update focus state in both old and newly focused block
   * @param old_index Block index with focus
   * @param new_index Block index to be focused
   */
  void UpdateFocus(int old_index, int new_index);

  /**
   * @brief Check if terminal is smaller than the minimum size to render all blocks (fullscreen
   * mode is not affected, as it renders only spectrum visualizer)
   * @return true if terminal is too small, otherwise false
   */
  bool IsTooSmall() const {
    return !fullscreen_mode_ && (size_.dimx < kMinColumns || size_.dimy < kMinLines);
  }

  /**
   * @brief Renders message asking user to resize terminal
   * @return UI element
   */
  ftxui::Element RenderTooSmall() const;

  /**
   * @brief Check for all dialogs if any is opened
   * @return true if any dialog is visible, otherwise false
   */
  bool IsDialogVisible() const {
    return error_dialog_->IsVisible() || help_dialog_->IsVisible() ||
           question_dialog_->IsVisible() || playlist_dialog_->IsVisible();
  }

  /**
   * @brief Get element to show as overlay (may be a dialog or nothing at all)
   */
  ftxui::Element GetOverlay() const;

  /* ******************************************************************************************** */
  //! Variables

  std::weak_ptr<audio::Notifier> notifier_;  //!< Audio notifier for events from UI

  //! Events to audio thread sent before audio notifier is registered (e.g. volume restored by
  //! media player while it is being created), handled once it gets registered
  std::vector<CustomEvent> pending_audio_events_;
  bool notifier_registered_ = false;  //!< Audio notifier was registered (it may be gone already)
  error::Code last_error_ = error::kSuccess;  //!< Last application error

  std::unique_ptr<ErrorDialog> error_dialog_;  //!< Dialog box to show customized error messages
  std::unique_ptr<HelpDialog> help_dialog_;    //!< Dialog box to show help menu
  std::unique_ptr<QuestionDialog> question_dialog_;  //!< Dialog box to question user
  std::unique_ptr<PlaylistDialog> playlist_dialog_;  //!< Dialog box to manage playlists
  std::unique_ptr<ThemePicker> theme_picker_;        //!< Picker to choose UI theme
  std::unique_ptr<DevicePicker> device_picker_;      //!< Picker to choose audio output device

  //! Custom event receiver
  ftxui::Receiver<CustomEvent> receiver_ = ftxui::MakeReceiver<CustomEvent>();
  ftxui::Sender<CustomEvent> sender_ = receiver_->MakeSender();  //! Custom event sender

  EventCallback cb_send_event_;  //!< Function to send custom events to terminal interface
  Callback cb_exit_;             //!< Function to exit from graphical interface

  //! Function to get terminal maximum size
  std::function<ftxui::Dimensions()> cb_size_ = ftxui::Terminal::Size;

  ftxui::Dimensions size_ = cb_size_();  //!< Terminal maximum size
  int focused_index_ = 0;                //!< Index of focused block

  bool global_mode_ = true;       //!< Control flag to process events in global mode
  bool fullscreen_mode_ = false;  //!< Control flag to show spectrum visualizer in fullscreen

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::TerminalTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_TERMINAL_H_
