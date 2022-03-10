/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_UI_BLOCK_LIST_DIR_H_
#define INCLUDE_UI_BLOCK_LIST_DIR_H_

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
class ListDir : public Block {
 public:
  /**
   * @brief Construct a new File List object
   *
   * @param init Initial coordinate based on screen portion(x,y)
   * @param size Screen portion size for this block
   */
  explicit ListDir(screen_portion_t init, screen_portion_t size);

  /**
   * @brief Destroy the File List object
   *
   */
  virtual ~ListDir() = default;

  /* ******************************************************************************************** */
 private:
  //! Possible states
  class InitialState;
};

/* ********************************************************************************************** */

class ListDir::InitialState : public Block::State {
 public:
  static ListDir::InitialState* GetInstance() { return new ListDir::InitialState; };
  void Init(Block& block) override;
  void Draw(Block& block) override;
  void HandleInput(Block& block, int key) override;

  void RefreshList(const std::filesystem::path& dir_path);

 private:
  void DrawItem(WINDOW* window, int index, const std::filesystem::path& item);

 private:
  std::filesystem::path curr_dir_;  //!< Current directory
  int highlighted_;                 //!< Index to highlight current file

  std::vector<std::filesystem::path> list_;  //!< List containing files from current directory
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_LIST_DIR_H_