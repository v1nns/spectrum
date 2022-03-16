/**
 * \file
 * \brief  Base class for a block in UI
 */

#ifndef INCLUDE_UI_BASE_BLOCK_H_
#define INCLUDE_UI_BASE_BLOCK_H_

#include <ncurses.h>

#include <functional>
#include <string>

#include "ui/common.h"

namespace interface {

/**
 * @brief Callback to set focus in the block instead of the global terminal
 */
using set_focus_callback = std::function<void(bool)>;

/**
 * @brief Base class to draw a single block in terminal (based on a Block-design User Interface)
 */
class Block {
 protected:
  //! Forward declaration of inner class
  class State;

  /* ******************************************************************************************** */
  /**
   * @brief Construct a new Block object (cannot instantiate directly, derived class must use it)
   *
   * @param init Initial point coordinate {x,y} based on screen portion
   * @param size Screen portion size for this block
   * @param title Title to be shown in border
   * @param state Initial block state
   */
  explicit Block(const screen_portion_t& init, const screen_portion_t& size,
                 const std::string& title, State* state);

 public:
  /**
   * @brief Destroy the Block object
   */
  virtual ~Block();

  /* ******************************************************************************************** */
  /**
   * @brief Initialize Block window
   *
   * @param max_size Maximum screen size from terminal
   */
  void Init(const screen_size_t& max_size);

  /**
   * @brief Destroy Block window
   */
  void Destroy();

  /**
   * @brief Register callback to set focus in the current block
   *
   * @param cb Terminal function bound to a callback function
   */
  void RegisterCallback(set_focus_callback cb);

  /**
   * @brief Resize Block window
   *
   * @param max_size Maximum screen size from terminal
   */
  void ResizeWindow(const screen_size_t& max_size);

  /**
   * @brief Get the Window object
   *
   * @return WINDOW* Current Window for this block
   */
  WINDOW* GetWindow() { return win_; };

  /**
   * @brief Get focus from the global terminal
   *
   * @param focused "true" to get focus, otherwise "false"
   */
  void GetFocus(bool focused);

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Calculate block screen size based on its portion size and maximum screen size
   *
   * @param max_size Maximum screen size from terminal
   */
  void CalculateScreenSize(const screen_size_t& max_size);

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Draw only border and title
   */
  void DrawBorder();

  /**
   * @brief Draw user interface, internally it will call implementation from derived-class
   */
  void Draw();

  /**
   * @brief Force to refresh on next drawing
   */
  void ForceRefresh();

  /**
   * @brief Handle keyboard input, internally it will call implementation from derived-class
   *
   * @param key Character corresponding to the key pressed
   */
  void HandleInput(int key);

  /* ******************************************************************************************** */
 protected:
  /**
   * @brief Inner class to represent a block state
   */
  class State {
   public:
    virtual ~State(){};
    virtual void Init(Block& block){};
    virtual void Draw(Block& block){};
    virtual void HandleInput(Block& block, int key){};
    virtual void Exit(Block& block){};

   protected:
    void ChangeState(Block& block, State* new_state) { block.ChangeState(new_state); }
  };

  /* ******************************************************************************************** */
  /**
   * @brief Change current block state
   *
   * @param new_state New block state
   */
  void ChangeState(State* new_state);

  /* ******************************************************************************************** */
 protected:
  screen_portion_t init_, size_;  //!< Defined screen size for this block
  point_t calc_init_;  //!< Calculated initial point coordinate using the defined screen portion
  screen_size_t calc_size_;  //!< Calculated screen size using the defined screen portion

  WINDOW *border_, *win_;  //!< NCURSES GUI windows for border and block content

  std::string border_title_;  //!< Text to be shown as title in border box

  set_focus_callback set_focus_;  //!< Callback to "steal" focus from the global terminal

  /* ******************************************************************************************** */
 private:
  State* curr_state_;  //!< Current block state
  bool refresh_;       //!< Force block to draw again
};

}  // namespace interface
#endif  // INCLUDE_UI_BASE_BLOCK_H_