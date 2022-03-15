/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_
#define INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "ui/base/block.h"
#include "ui/common.h"

namespace interface {

/**
 * @brief Class for List files in directory block
 */
class ListDirectory : public Block {
 public:
  /**
   * @brief Construct a new File List object
   *
   * @param init Initial coordinate based on screen portion(x,y)
   * @param size Screen portion size for this block
   */
  explicit ListDirectory(screen_portion_t init, screen_portion_t size);

  /**
   * @brief Destroy the File List object
   *
   */
  virtual ~ListDirectory() = default;

  /* ******************************************************************************************** */
 private:
  //! Possible states
  class InitialState;
};

/* ********************************************************************************************** */

class ListDirectory::InitialState : public Block::State {
 public:
  static ListDirectory::InitialState* GetInstance() { return new ListDirectory::InitialState; };
  void Init(Block& block) override;
  void Draw(Block& block) override;
  void HandleInput(Block& block, int key) override;

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Represent a single file entry
   */
  struct Item {
    std::string path;
    bool is_dir;
    bool is_hover;
    bool contains_search_term;
  };

  //! For better readability
  using ItemPtr = std::vector<Item>::iterator;
  using Items = std::vector<Item>;

  /**
   * @brief Get active item object
   *
   * @return ItemPtr Current item on hover
   */
  ItemPtr GetActiveItem();

  /**
   * @brief Get a list with filtered items to print
   *
   * @param max_rows Maximum number of rows available to print
   * @return Items Vector of items
   */
  Items GetItemsToPrint(int max_rows);

  /**
   * @brief Refresh list with files from new or current directory
   *
   * @param dir_path Full path to directory
   */
  void RefreshList(const std::filesystem::path& dir_path);

  /* ******************************************************************************************** */
  /**
   * @brief Draw current directory
   *
   * @param window Pointer to Block window
   */
  void DrawCurrentDirectory(WINDOW* window);

  /**
   * @brief Draw (or more accurately, print) a single item entry
   *
   * @param window Pointer to Block window
   * @param row Row index to draw
   * @param item Item entry (may be a file or a directory)
   */
  void DrawItem(WINDOW* window, int row, const Item& item);

  /**
   * @brief Draw search box with the text to be search through the list
   *
   * @param window Pointer to Block window
   * @param max_rows Maximum row index for the current Block window
   */
  void DrawSearchBox(WINDOW* window, int max_rows);

  /* ******************************************************************************************** */

  /**
   * @brief Add new character to search term and update all items containing the same string
   *
   * @param key Character corresponding to the key pressed
   */
  void UpdateTextToSearch(int key = -1);

  /* ******************************************************************************************** */
 private:
  std::filesystem::path curr_dir_;  //!< Current directory
  Items list_;                      //!< List containing files from current directory

  bool search_mode_;            //!< Flag to enable search mode
  std::string text_to_search_;  //!< Custom text to search in files list
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_