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

class Dispatcher : public std::enable_shared_from_this<Dispatcher> {
 public:
  virtual ~Dispatcher() = default;

  virtual void Add(std::shared_ptr<Block> const) = 0;
  virtual void Broadcast(std::shared_ptr<Block> const, BlockEvent) = 0;

 protected:
  Dispatcher() = default;
};

/**
 * @brief Base class that manages the whole screen and contains all blocks
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
  void Add(std::shared_ptr<Block> const b) override;

  //! As a mediator, send a block event for every other block
  void Broadcast(std::shared_ptr<Block> const sender, BlockEvent event) override;

  /* ******************************************************************************************** */
 private:
  std::vector<std::shared_ptr<Block>> blocks_;  //!< List of all blocks that composes the interface
  Component container_;                         //!< The glue that holds the blocks together
};

}  // namespace interface
#endif  // INCLUDE_UI_BASE_TERMINAL_H_