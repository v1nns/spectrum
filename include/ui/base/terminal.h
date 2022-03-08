/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_UI_TERMINAL_H_
#define INCLUDE_UI_TERMINAL_H_

#include <ncurses.h>

#include <memory>
#include <vector>

#include "ui/base/block.h"
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
  Terminal() : max_size_({0, 0}), blocks_(), exit_(false){};

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
   * @brief Resize terminal window triggered by a SIGWINCH event from NCURSES
   */
  void OnResize();

  /**
   * @brief Polling for keyboard input event
   */
  void OnPolling();

  /**
   * @brief Draw user interface, internally will trigger every block to draw its content
   */
  void OnDraw();

  /* ******************************************************************************************** */
  /**
   * @brief Append a new block to be shown in screen
   *
   * @param b Pointer to Block-derived class
   */
  void AppendBlock(std::unique_ptr<Block>& b);

  /* ******************************************************************************************** */
  // TODO: document
  bool Tick(volatile bool& resize);

  /* ******************************************************************************************** */
  /**
   * @brief Get current screen size from terminal screen
   *
   * @return screen_size_t Current size {column,row}
   */
  screen_size_t GetCurrentScreenSize();

  /* ******************************************************************************************** */
 private:
  screen_size_t max_size_;  //!< Maximum terminal screen size

  std::vector<std::unique_ptr<Block>> blocks_;  //!< Vector of blocks shown in screen

  bool exit_;  //!< Force application exit
};

}  // namespace interface
#endif  // INCLUDE_UI_TERMINAL_H_