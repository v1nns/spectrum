/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_UI_MODULE_FILE_INFO_H_
#define INCLUDE_UI_MODULE_FILE_INFO_H_

#include <string>
#include <vector>

#include "sound/wave.h"
#include "ui/block.h"
#include "ui/common.h"

namespace interface {

/**
 * @brief Class for File Information module
 */
class FileInfo : public Block {
 public:
  /**
   * @brief Construct a new File Info object
   *
   * @param init Initial point(x,y)
   * @param size Screen size for this block
   */
  explicit FileInfo(point_t init, screen_size_t size);

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

  /* ******************************************************************************************** */
 private:
  WaveFormat song_;  //!< File information from song in WAVE format
};

/* ********************************************************************************************** */

class FileInfo::InitialState : public Block::BlockState {
 public:
  static FileInfo::InitialState* GetInstance() { return new FileInfo::InitialState; };
  void Draw(Block& block) override;
  void HandleInput(Block& block, char key) override;
};

/* ********************************************************************************************** */

class FileInfo::ShowInfoState : public Block::BlockState {
 public:
  static FileInfo::ShowInfoState* GetInstance() { return new FileInfo::ShowInfoState; };
  void Draw(Block& block) override;
};

}  // namespace interface
#endif  // INCLUDE_UI_MODULE_FILE_INFO_H_