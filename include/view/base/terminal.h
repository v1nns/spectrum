/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_VIEW_BASE_TERMINAL_H_
#define INCLUDE_VIEW_BASE_TERMINAL_H_

#include <memory>
#include <vector>

#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component_base.hpp"  // for Component
#include "ftxui/component/receiver.hpp"
#include "middleware/media_controller.h"
#include "model/application_error.h"
#include "model/block_identifier.h"
#include "view/base/block.h"
#include "view/base/custom_event.h"
#include "view/base/event_dispatcher.h"
#include "view/element/error_dialog.h"
#include "view/element/help.h"

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
 private:
  /**
   * @brief Construct a new Terminal object
   */
  Terminal();

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return Terminal object
   * @return std::shared_ptr<Terminal> Terminal instance
   */
  static std::shared_ptr<Terminal> Create();

  /**
   * @brief Destroy the Terminal object
   */
  virtual ~Terminal();

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
   */
  void Init();

  /**
   * @brief Force application to exit
   */
  void Exit();

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
   * @return Number of bars
   */
  int CalculateNumberBars();

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
   * @brief Handle event to switch block focus
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool HandleEventToSwitchBlockFocus(const ftxui::Event& event);

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
  bool HandleEventFromAudioThreadToInterface(const CustomEvent& event);

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
  void SetApplicationError(error::Code id) override;

  /* ******************************************************************************************** */
  //! Utils
 private:
  //! Get internal block index based on block identifier
  int GetIndexFromBlockIdentifier(const model::BlockIdentifier& id);

  /* ******************************************************************************************** */
  //! Default Constants
 private:
  static constexpr int kMaxBlocks = 4;  //!< Maximum number of blocks (used for focus control)

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::weak_ptr<audio::Notifier> notifier_;  //!< Audio notifier for events from UI
  error::Code last_error_;                   //!< Last application error

  std::unique_ptr<ErrorDialog> error_dialog_;  //!< Dialog box to show customized error messages
  std::unique_ptr<Help> helper_;               //!< Dialog box to show help menu

  ftxui::Receiver<CustomEvent> receiver_;  //! Custom event receiver
  ftxui::Sender<CustomEvent> sender_;      //! Custom event sender

  EventCallback cb_send_event_;  //!< Function to send custom events to terminal interface
  Callback cb_exit_;             //!< Function to exit from graphical interface

  ftxui::Dimensions size_;  //!< Terminal maximum size
  int focused_index_;       //!< Index of focused block
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_TERMINAL_H_
