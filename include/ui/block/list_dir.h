/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_UI_MODULE_FILE_LIST_H_
#define INCLUDE_UI_MODULE_FILE_LIST_H_

#include <string>
#include <vector>

#include "ui/base/block.h"
#include "ui/common.h"

namespace interface {

/**
 * @brief Class for File List module
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

  void RefreshList();

 private:
  std::string curr_dir_;            //!< Current directory
  std::vector<std::string> files_;  //!< List containing files from current directory
  int highlighted_;
};

}  // namespace interface
#endif  // INCLUDE_UI_MODULE_FILE_LIST_H_