/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_UI_BLOCK_FILE_INFO_H_
#define INCLUDE_UI_BLOCK_FILE_INFO_H_

#include "sound/wave.h"

namespace interface {

/**
 * @brief Class for File Information block
 */
class FileInfo {
 public:
  /**
   * @brief Construct a new File Info object
   *
   * @param init Initial coordinate based on screen portion(x,y)
   * @param size Screen portion size for this block
   */
  explicit FileInfo();

  /**
   * @brief Destroy the File Info object
   *
   */
  virtual ~FileInfo() = default;
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_FILE_INFO_H_