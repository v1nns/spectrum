/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_VIEW_BASE_TERMINAL_H_
#define INCLUDE_VIEW_BASE_TERMINAL_H_

#include <memory>
#include <vector>

#include "controller/player.h"
#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component_base.hpp"  // for Component
#include "model/application_error.h"
#include "view/base/block.h"
#include "view/base/block_event.h"
#include "view/base/event_dispatcher.h"

namespace interface {

/**
 * @brief Class that manages the whole screen and contains all blocks
 */
class Terminal : public EventDispatcher, public ftxui::ComponentBase {
 public:
  /**
   * @brief Construct a new Terminal object
   */
  Terminal();

  /**
   * @brief Destroy the Terminal object
   */
  virtual ~Terminal();

  /* ******************************************************************************************** */

  //! Remove these
  Terminal(const Terminal& other) = delete;             // copy constructor
  Terminal(Terminal&& other) = delete;                  // move constructor
  Terminal& operator=(const Terminal& other) = delete;  // copy assignment
  Terminal& operator=(Terminal&& other) = delete;       // move assignment

  /* ******************************************************************************************** */

  /**
   * @brief Initialize screen for Terminal object
   */
  void Init();

  /**
   * @brief Force application to exit
   */
  void Exit();

  /**
   * @brief Bind an external exit function to an internal function
   * @param cb Callback function to exit graphical application
   */
  void RegisterExitCallback(Callback cb);

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event) override;

 private:
  /**
   * @brief Handles a event when no internal mode has been set
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnGlobalModeEvent(ftxui::Event event);

  /**
   * @brief Handles a event while on error mode (it means someone informed an error)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnErrorModeEvent(ftxui::Event event);

  /* ******************************************************************************************** */
 public:
  //! As a mediator, send a block event for every other block
  void Broadcast(Block* sender, BlockEvent event) override;

  //! Set application error (can be originated from controller or interface::block)
  void SetApplicationError(error::Code id) override;

  /* ******************************************************************************************** */
 private:
  std::shared_ptr<controller::Player> player_;  //!< Player controller
  std::optional<error::Code> last_error_;       //!< Last application error that has occurred

  Callback cb_exit_;  //!< Function to exit from graphical interface
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_TERMINAL_H_