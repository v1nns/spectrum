/**
 * \file
 * \brief  Base class for a block in UI
 */

#ifndef INCLUDE_UI_BLOCK_H_
#define INCLUDE_UI_BLOCK_H_

#include <curses.h>

#include <string>

#include "ui/common.h"

namespace interface {

/**
 * @brief Base class to draw a single block in terminal (based on a Block-design User Interface)
 */
class Block {
 protected:
  //! Forward declaration of inner class
  class BlockState;

  /* ******************************************************************************************** */
  /**
   * @brief Construct a new Block object (cannot instantiate directly, derived class must use it)
   *
   * @param init Initial point coordinate {x,y}
   * @param size Screen portion size for this block
   * @param title Title to be shown in border
   * @param state Initial block state
   */
  explicit Block(const point_t& init, const screen_portion_t& size, const std::string& title,
                 BlockState* state);

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

  /* ******************************************************************************************** */
  /**
   * @brief Draw only border and title
   */
  void DrawBorder();

  /**
   * @brief Draw user interface, internally it will call implementation from derived-class
   */
  void Draw();

  /**
   * @brief Handle keyboard input, internally it will call implementation from derived-class
   *
   * @param key Character corresponding to the key pressed
   */
  void HandleInput(char key);

  /* ******************************************************************************************** */
 protected:
  /**
   * @brief Inner class to represent a block state
   */
  class BlockState {
   public:
    virtual ~BlockState(){};
    virtual void Draw(Block& block){};
    virtual void HandleInput(Block& block, char key){};

   protected:
    void ChangeState(Block& block, BlockState* new_state) { block.ChangeState(new_state); }
  };

  /* ******************************************************************************************** */
  /**
   * @brief Change current block state
   *
   * @param new_state New block state
   */
  void ChangeState(BlockState* new_state);

  /* ******************************************************************************************** */
 protected:
  point_t init_;           //!< Initial point for this block
  screen_portion_t size_;  //!< Defined screen size for this block

  WINDOW *border_, *win_;  //!< NCURSES GUI windows for border and block content

  std::string border_title_;  //!< Text to be shown as title in border box

  /* ******************************************************************************************** */
 private:
  BlockState* curr_state_;  //!< Current block state
  bool refresh_;            //!< Force block to draw again
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_H_