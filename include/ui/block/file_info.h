/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_UI_BLOCK_FILE_INFO_H_
#define INCLUDE_UI_BLOCK_FILE_INFO_H_

#include "sound/wave.h"
#include "ui/base/block.h"
#include "ui/common.h"

namespace interface {

/**
 * @brief Class for File Information block
 */
class FileInfo : public Block {
 public:
  /**
   * @brief Construct a new File Info object
   *
   * @param init Initial coordinate based on screen portion(x,y)
   * @param size Screen portion size for this block
   */
  explicit FileInfo(screen_portion_t init, screen_portion_t size);

  /**
   * @brief Destroy the File Info object
   *
   */
  virtual ~FileInfo() = default;

  /* ******************************************************************************************** */
 private:
  //! Possible states
  class InitialState;
  class ShowInfoState;
};

/* ********************************************************************************************** */

class FileInfo::InitialState : public Block::State {
 public:
  static FileInfo::InitialState* GetInstance() { return new FileInfo::InitialState; };
  void Draw(Block& block) override;
  void HandleInput(Block& block, int key) override;
};

/* ********************************************************************************************** */

class FileInfo::ShowInfoState : public Block::State {
 public:
  static FileInfo::ShowInfoState* GetInstance() { return new FileInfo::ShowInfoState; };
  void Init(Block& block) override;
  void Draw(Block& block) override;

 private:
  WaveFormat song_;  //!< File information from song in WAVE format
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_FILE_INFO_H_