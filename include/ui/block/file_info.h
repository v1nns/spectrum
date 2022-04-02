/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_UI_BLOCK_FILE_INFO_H_
#define INCLUDE_UI_BLOCK_FILE_INFO_H_

#include <memory>

#include "sound/wave.h"
#include "ui/base/block.h"

namespace interface {

using namespace ftxui;

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
 */
class FileInfo : public Block {
 public:
  /**
   * @brief Construct a new File Info object
   * @param d Block event dispatcher
   */
  explicit FileInfo(const std::shared_ptr<Dispatcher>& d);

  /**
   * @brief Destroy the File Info object
   */
  virtual ~FileInfo() = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   *
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(Event event) override;

  /**
   * @brief Handles an event (from another block)
   * @param event Received event from dispatcher
   */
  void OnBlockEvent(BlockEvent event) override;

  /* ******************************************************************************************** */
 private:
  // TODO: change name and document
  void ReadMusicFile(std::string path);

  /* ******************************************************************************************* */
 private:
  std::unique_ptr<Song> file_;
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_FILE_INFO_H_