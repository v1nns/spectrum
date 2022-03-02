/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_UI_TERMINAL_H_
#define INCLUDE_UI_TERMINAL_H_

#include <curses.h>

#include <memory>
#include <vector>

#include "ui/block.h"
#include "ui/common.h"

namespace interface {

/**
 * @brief Class that represents the whole screen
 */
class Terminal {
 public:
  /**
   * @brief Construct a new Terminal object
   */
  Terminal() : win_(), max_size_({0, 0}), blocks_(), exit_(false), last_key_{0} {};

  /**
   * @brief Destroy the Terminal object
   */
  virtual ~Terminal() { Destroy(); };

  /* ******************************************************************************************** */
  //! Remove these constructors/operators
  Terminal(const Terminal& other) = delete;             // copy constructor
  Terminal(Terminal&& other) = delete;                  // move constructor
  Terminal& operator=(const Terminal& other) = delete;  // copy assignment
  Terminal& operator=(Terminal&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  /**
   * @brief Initialize screen for Terminal object
   *
   * @return int Error code from operation
   */
  int Init();

  /**
   * @brief Destroy screen used by Terminal object
   *
   * @return int Error code from operation
   */
  int Destroy();

  /* ******************************************************************************************** */
  /**
   * @brief Append a new block to be shown in screen
   *
   * @param b Pointer to Block-derived class
   */
  void AppendBlock(std::unique_ptr<Block>& b);

  /* ******************************************************************************************** */
  // TODO: document
  bool Tick();

  /**
   * @brief Polling for keyboard input
   */
  void PollingInput();

  /* ******************************************************************************************** */
  /**
   * @brief Check if screen size from the running terminal has changed
   *
   * @return true If size has changed
   * @return false If size has not changed
   */
  bool IsDimensionUpdated();

  // TODO: document
  void Draw();

  /* ******************************************************************************************** */
 private:
  WINDOW* win_;             //!< NCURSES GUI windows for screen content
  screen_size_t max_size_;  //!< Maximum screen size

  std::vector<std::unique_ptr<Block>> blocks_;  //!< Vector of blocks shown in screen

  bool exit_;  //!< Force application exit

  char last_key_;  //!< Last key pressed on keyboard
};

}  // namespace interface
#endif  // INCLUDE_UI_TERMINAL_H_