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

class FileInfo : public Block {
 public:
  explicit FileInfo(point_t init, screen_size_t size) : Block(init, size, "File Information"){};
  ~FileInfo() = default;

  void Draw(bool rescale) override;
  void HandleInput(char key) override;

 private:
  WaveFormat song_;
};

}  // namespace interface
#endif  // INCLUDE_UI_MODULE_FILE_INFO_H_