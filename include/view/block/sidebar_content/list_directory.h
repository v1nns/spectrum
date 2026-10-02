/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_
#define INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ftxui/dom/elements.hpp"
#include "model/playlist.h"
#include "util/file_handler.h"
#include "view/base/block.h"
#include "view/element/menu.h"
#include "view/element/tab.h"

#ifdef ENABLE_TESTS
#include <gtest/gtest_prod.h>

//! Forward declaration
namespace {
class SidebarTest;
class ListDirectoryCtorTest;
}  // namespace
#endif

namespace interface {

/**
 * @brief Component to list files from given directory
 */
class ListDirectory : public TabItem {
  static constexpr std::string_view kTabName = "files";  //!< Tab title

 public:
  //! Callback to check if given file contains an audio stream
  using AudioCheckCallback = std::function<bool(const util::File& file)>;

  /**
   * @brief Construct a new ListDirectory object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   * @param on_focus Callback function to ask for focus
   * @param keybinding Keybinding to set item as active
   * @param file_handler Utility handler to manage any file operation
   * @param max_columns Maximum number of visual columns to be used by this element
   * @param optional_path Custom directory path to fill initial list of files
   * @param contains_audio_cb Callback to check if selected file contains audio stream before
   *                          asking to play it (if empty, no check is done)
   */
  explicit ListDirectory(const model::BlockIdentifier& id,
                         const std::shared_ptr<EventDispatcher>& dispatcher,
                         const FocusCallback& on_focus, const keybinding::Key& keybinding,
                         const std::shared_ptr<util::FileHandler>& file_handler, int max_columns,
                         const std::string& optional_path = "",
                         const AudioCheckCallback& contains_audio_cb = nullptr);

  /**
   * @brief Destroy the List Directory object
   */
  ~ListDirectory() override = default;

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
  bool OnEvent(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event& event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /**
   * @brief Called when this tab item becomes active (or its parent block gets focus), to read
   * current directory again and update list with any file change
   */
  void OnFocus() override;

  /* ******************************************************************************************** */
  //! File list operations
 private:
  /**
   * @brief Create queue with media files from list to play, starting from the given file
   * @param file Filepath selected to play
   * @return Queue of songs (selected file, then every other media file from list, wrapping around)
   */
  model::Playlist CreateQueue(const util::File& file);

  /**
   * @brief Send file selection to be played by audio thread. If file does not contain an audio
   * stream, it is not sent (to not interrupt current song) and an error is shown instead
   * @param file Filepath
   * @return true if file selection was handled (sent or error shown), otherwise false
   */
  bool SendFileSelection(const util::File& file);

  /* ******************************************************************************************** */
  //! Local cache for current active information
 protected:
  //! Get current directory
  const std::filesystem::path& GetCurrentDir() const { return menu_->actual().GetCurrentDir(); }

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::optional<std::filesystem::path> curr_playing_ = std::nullopt;  //!< Current song playing

  int max_columns_;  //!< Maximum number of columns (characters in a single line) available to use

  AudioCheckCallback contains_audio_cb_;  //!< Check if file contains audio before playing it

  FileMenu menu_;  //!< Menu with a list of files

  /* ******************************************************************************************** */
  //! Friend test

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
  friend class ::ListDirectoryCtorTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_
