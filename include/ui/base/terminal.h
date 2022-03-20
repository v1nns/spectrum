/**
 * \file
 * \brief  Class representing the whole terminal
 */

#ifndef INCLUDE_UI_BASE_TERMINAL_H_
#define INCLUDE_UI_BASE_TERMINAL_H_

#include <ncurses.h>

#include <memory>
#include <optional>
#include <vector>

#include "error/error_table.h"
#include "ui/base/block.h"
#include "ui/common.h"

namespace interface {

/**
 * @brief Base class that manages the whole screen and hold all blocks
 */
class Terminal {
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
  void Init();

  /**
   * @brief Destroy screen used by Terminal object
   *
   * @return int Error code from operation
   */
  void Destroy();

  /**
   * @brief Force application to exit
   */
  void Exit();

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Initialize colors used throughout the application
   */
  void InitializeColors();

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
   * @brief Handle keyboard input for global commands
   *
   * @param key Character corresponding to the key pressed
   */
  void HandleInput(int key);

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Set a critical error to exit application
   *
   * @param err_code Unique error code
   */
  void SetCriticalError(int err_code);

  /**
   * @brief Set/unset focus to the child block
   *
   * @param focused Flag indicating if block get focused
   */
  void SetFocus(bool focused);

  /**
   * @brief Append a new block to be shown in screen
   *
   * @param b Pointer to Block-derived class
   */
  void AppendBlock(std::unique_ptr<Block>& block);

  /* ******************************************************************************************** */
  /**
   * @brief Main loop for the graphical interface
   *
   * @return true To exit from application
   * @return false To keep running
   */
  bool Tick();

  /* ******************************************************************************************** */
  /**
   * @brief Get maximum screen size for terminal screen
   *
   * @return screen_size_t Size{column,row}
   */
  screen_size_t GetScreenSize();

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Global hook to filter received signals
   *
   * @param sig Signal event received
   */
  static void SignalHook(int sig);

  static bool resize_screen_;  //!< Flag to control if must resize the window

  /* ******************************************************************************************** */
 private:
  screen_size_t max_size_;                      //!< Maximum terminal screen size
  std::vector<std::unique_ptr<Block>> blocks_;  //!< Vector of blocks shown in screen

  bool has_focus_;  //!< Flag to control if must execute global commands

  std::optional<error::message_t> critical_error_;  //!< Critical error that forces application exit
  bool exit_;  //!< Indicate if application must gracefully exit
};

}  // namespace interface
#endif  // INCLUDE_UI_BASE_TERMINAL_H_