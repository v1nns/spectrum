/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_VIEW_BLOCK_FILE_INFO_H_
#define INCLUDE_VIEW_BLOCK_FILE_INFO_H_

#include <memory>  // for shared_ptr, unique_ptr
#include <string>  // for string

#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/dom/elements.hpp"              // for Element
#include "model/song.h"                        // for Song
#include "view/base/block.h"                   // for Block, BlockEvent (ptr...

namespace interface {

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
 */
class FileInfo : public Block {
 public:
  /**
   * @brief Construct a new File Info object
   * @param dispatcher Block event dispatcher
   */
  explicit FileInfo(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the File Info object
   */
  virtual ~FileInfo() = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************* */
 private:
  model::Song audio_info_;  //!< Audio information from current song
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_FILE_INFO_H_
