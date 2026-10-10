/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_VIEW_BLOCK_FILE_INFO_H_
#define INCLUDE_VIEW_BLOCK_FILE_INFO_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ftxui/dom/elements.hpp"
#include "model/audio_output.h"
#include "model/song.h"
#include "view/base/block.h"
#include "view/element/style.h"

namespace interface {

/**
 * @brief Component with detailed information about the chosen file (in this case, some music file)
 */
class FileInfo : public Block {
  static constexpr int kMaxColumns = kLeftColumnWidth;  //!< Maximum columns for Component
  static constexpr int kMaxRows = 11;                   //!< Maximum rows for Component
  static constexpr int kFieldColumns = 9;  //!< Columns for field name (and space after it)

  static constexpr std::string_view kUnknown = "—";  //!< Value not informed by song or player

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

  /**
   * @brief Parse audio output into internal cache to render on UI later (after song information)
   * @param output Audio output used to play current song (nothing when there is no song)
   */
  void ParseAudioOutput(const std::optional<model::AudioOutput>& output);

  /* ******************************************************************************************* */
  //! Variables
 private:
  using Entry = std::pair<std::string, std::string>;  //!< A pair of <Field,Value>
  std::string title_;                                 //!< Song title (or its source, without one)
  std::string artist_;                                //!< Song artist
  std::vector<Entry> audio_info_;                     //!< Parsed audio information to render on UI
  std::vector<Entry> output_info_;                    //!< Parsed audio output to render on UI

  bool has_song_info_ = false;  //!< Flag to indicate if displaying information from a song
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_FILE_INFO_H_
