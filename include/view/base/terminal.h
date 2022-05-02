/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_VIEW_BASE_TERMINAL_H_
#define INCLUDE_VIEW_BASE_TERMINAL_H_

#include <memory>
#include <vector>

// #include "error/error_table.h"
#include "controller/player.h"
#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component_base.hpp"  // for Component
#include "view/base/block.h"
#include "view/base/block_event.h"
#include "view/base/event_dispatcher.h"

namespace interface {

/**
 * @brief Class that manages the whole screen and contains all blocks
 */
class Terminal : public EventDispatcher {
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
   * @brief Main loop for the graphical interface
   */
  void Loop();

  /* ******************************************************************************************** */

  //! Push back new block to internal vector
  void Add(const std::shared_ptr<Block>& b) override;

  //! As a mediator, send a block event for every other block
  void Broadcast(Block* sender, BlockEvent event) override;

  /* ******************************************************************************************** */
 private:
  std::shared_ptr<controller::Player> player_;  //!< Player controller
  std::vector<std::shared_ptr<Block>> blocks_;  //!< List of all blocks composing the interface

  ftxui::Component container_;  //!< The glue that holds the blocks together
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_TERMINAL_H_