/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_UI_BASE_TERMINAL_H_
#define INCLUDE_UI_BASE_TERMINAL_H_

#include <memory>
#include <vector>

// #include "error/error_table.h"
#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component_base.hpp"  // for Component
#include "ui/base/block.h"

namespace interface {

using namespace ftxui;

/**
 * @brief Base class to dispatch events among the different blocks
 */
class Dispatcher : public std::enable_shared_from_this<Dispatcher> {
 public:
  virtual ~Dispatcher() = default;

  virtual void Add(const std::shared_ptr<Block>&) = 0;
  virtual void Broadcast(Block*, BlockEvent) = 0;

 protected:
  Dispatcher() = default;
};

/**
 * @brief Class that manages the whole screen and contains all blocks
 */
class Terminal : public Dispatcher {
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
  std::vector<std::shared_ptr<Block>> blocks_;  //!< List of all blocks composing the interface
  Component container_;                         //!< The glue that holds the blocks together
};

}  // namespace interface
#endif  // INCLUDE_UI_BASE_TERMINAL_H_