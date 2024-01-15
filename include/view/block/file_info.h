/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_VIEW_BLOCK_FILE_INFO_H_
#define INCLUDE_VIEW_BLOCK_FILE_INFO_H_

#include <memory>

#include "ftxui/dom/elements.hpp"
#include "model/song.h"
#include "view/base/block.h"

namespace interface {

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
 */
class FileInfo : public Block {
  static constexpr int kMaxRows = 15;  //!< Maximum rows for the Component

 public:
  /**
   * @brief Construct a new File Info object
   * @param dispatcher Block event dispatcher
   */
  explicit FileInfo(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the File Info object
   */
  ~FileInfo() override = default;

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
