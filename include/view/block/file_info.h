/**
 * \file
 * \brief  Class for block containing file info
 */

#ifndef INCLUDE_VIEW_BLOCK_FILE_INFO_H_
#define INCLUDE_VIEW_BLOCK_FILE_INFO_H_

#include <functional>
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

  static constexpr std::string_view kUnknown = "—";     //!< Value not informed by song or player
  static constexpr std::string_view kFieldUrl = "url";  //!< Field with URL from streamed song

 public:
  //! Open URL in another application (return false if it could not be opened)
  using UrlOpener = std::function<bool(const std::string&)>;

  /**
   * @brief Construct a new File Info object
   * @param dispatcher Block event dispatcher
   * @param opener Callback to open URL from streamed song (if null, it is opened by xdg-open)
   */
  explicit FileInfo(const std::shared_ptr<EventDispatcher>& dispatcher, UrlOpener opener = nullptr);

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
  //! Internal handling
 private:
  /**
   * @brief Handle mouse event on URL from streamed song (hover and click to open it)
   * @param event Received mouse event
   * @return true if event was handled, otherwise false
   */
  bool OnUrlMouseEvent(ftxui::Event& event);

  /* ******************************************************************************************* */
  //! Variables
  using Entry = std::pair<std::string, std::string>;  //!< A pair of <Field,Value>
  std::string title_;                                 //!< Song title (or its source, without one)
  std::string artist_;                                //!< Song artist
  std::vector<Entry> audio_info_;                     //!< Parsed audio information to render on UI
  std::vector<Entry> output_info_;                    //!< Parsed audio output to render on UI

  bool has_song_info_ = false;  //!< Flag to indicate if displaying information from a song

  std::string url_;              //!< Full URL from streamed song (empty for a local file)
  ftxui::Box url_box_;           //!< Area where URL is rendered (to be clicked)
  bool is_url_hovered_ = false;  //!< Mouse is over URL
  UrlOpener url_opener_;         //!< Callback to open URL
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_FILE_INFO_H_
