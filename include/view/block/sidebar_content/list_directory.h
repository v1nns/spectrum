/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_
#define INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ftxui/dom/elements.hpp"
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
  /**
   * @brief Construct a new ListDirectory object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   * @param on_focus Callback function to ask for focus
   * @param keybinding Keybinding to set item as active
   * @param file_handler Utility handler to manage any file operation
   * @param max_columns Maximum number of visual columns to be used by this element
   * @param optional_path Custom directory path to fill initial list of files
   */
  explicit ListDirectory(const model::BlockIdentifier& id,
                         const std::shared_ptr<EventDispatcher>& dispatcher,
                         const FocusCallback& on_focus, const keybinding::Key& keybinding,
                         const std::shared_ptr<util::FileHandler>& file_handler, int max_columns,
                         const std::string& optional_path = "");

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

  /* ******************************************************************************************** */
 private:
  //! Getter for Title (for testing purposes, may be overridden)
  virtual std::string GetTitle();

  /* ******************************************************************************************** */
  //! File list operations

  /**
   * @brief Compose directory path to list files from (based on given path)
   * @param optional_path Path to list files
   * @return path Directory path
   */
  std::filesystem::path ComposeDirectoryPath(const std::string& optional_path);

  /**
   * @brief Refresh list with all files from the given directory path
   * @param dir_path Full path to directory
   * @return true if directory was parsed succesfully, false otherwise
   */
  bool RefreshList(const std::filesystem::path& dir_path);

  /**
   * @brief Select file to play based on the current song playing
   * @param pick_next Pick next or previous file to play
   * @return Filepath
   */
  util::File SelectFileToPlay(bool pick_next);

  /* ******************************************************************************************** */
  //! Local cache for current active information
 protected:
  //! Get current directory
  const std::filesystem::path& GetCurrentDir() const { return curr_dir_; }

  /* ******************************************************************************************** */
  //! Variables

 private:
  std::filesystem::path curr_dir_;                                    //!< Current directory
  std::optional<std::filesystem::path> curr_playing_ = std::nullopt;  //!< Current song playing

  int max_columns_;  //!< Maximum number of columns (characters in a single line) available to use
  std::shared_ptr<util::FileHandler> file_handler_;  //!< Utility class to manage files (read/write)

  FileMenu menu_;  //!< UI element to show files in a menu

  /* ******************************************************************************************** */
  //! Friend test

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
  friend class ::ListDirectoryCtorTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_
