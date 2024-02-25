/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_VIEW_BLOCK_FILE_INFO_H_
#define INCLUDE_VIEW_BLOCK_FILE_INFO_H_

#include <memory>
#include <utility>
#include <vector>

#include "ftxui/dom/elements.hpp"
#include "model/song.h"
#include "view/base/block.h"

namespace interface {

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
 */
class FileInfo : public Block {
  static constexpr int kMaxColumns = 36;  //!< Maximum columns for Component
  static constexpr int kMaxRows = 15;     //!< Maximum rows for Component

  static constexpr int kMaxSongLines = 8;  //!< Always remember to check song::to_string

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
  //! Utils

  /**
   * @brief Parse audio information into internal cache to render on UI later
   * @param audio Detailed audio information
   */
  void ParseAudioInfo(const model::Song& audio);

  /* ******************************************************************************************* */
  //! Variables
 private:
  using Entry = std::pair<std::string, std::string>;  //!< A pair of <Field,Value>
  std::vector<Entry> audio_info_;                     //!< Parsed audio information to render on UI

  bool is_song_playing_ = false;  //!< Flag to control when a song is playing
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_FILE_INFO_H_
