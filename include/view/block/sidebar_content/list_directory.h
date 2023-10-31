/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_
#define INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>  // for path
#include <memory>      // for shared_ptr
#include <mutex>
#include <optional>  // for optional
#include <string>    // for string, allocator
#include <string_view>
#include <thread>
#include <vector>  // for vector

#include "ftxui/component/captured_mouse.hpp"     // for ftxui
#include "ftxui/component/component_options.hpp"  // for MenuEntryOption
#include "ftxui/dom/elements.hpp"                 // for Element
#include "ftxui/screen/box.hpp"                   // for Box
#include "util/file_handler.h"
#include "view/base/block.h"  // for Block, BlockEvent...
#include "view/element/style.h"
#include "view/element/tab.h"
#include "view/element/text_animation.h"

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
  static constexpr int kMaxIconColumns = 2;              //!< Maximum columns for Icon

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
  //! Handle mouse wheel event
  bool OnMouseWheel(ftxui::Event event);

  //! Handle keyboard event mapped to a menu navigation command
  bool OnMenuNavigation(const ftxui::Event& event);

  //! Handle keyboard event when search mode is enabled
  bool OnSearchModeEvent(const ftxui::Event& event);

  /* ******************************************************************************************** */

  //! Getter for entries size
  int Size() const {
    return mode_search_ ? (int)mode_search_->entries.size() : (int)entries_.size();
  }

  //! Getter for selected index
  int* GetSelected() { return mode_search_ ? &mode_search_->selected : &selected_; }

  //! Getter for focused index
  int* GetFocused() { return mode_search_ ? &mode_search_->focused : &focused_; }

  //! Getter for entry at informed index
  util::File& GetEntry(int i) {
    return mode_search_ ? mode_search_->entries.at(i) : entries_.at(i);
  }

  //! Getter for active entry (focused/selected)
  util::File* GetActiveEntry() {
    if (!Size()) return nullptr;

    return mode_search_ ? &mode_search_->entries.at(mode_search_->selected)
                        : &entries_.at(selected_);
  }

  //! Clamp both selected and focused indexes
  void Clamp();

  //! Getter for Title (for testing purposes, may be overridden)
  virtual std::string GetTitle();

  /* ******************************************************************************************** */
  //! File list operations

  /**
   * @brief Refresh list with all files from the given directory path
   * @param dir_path Full path to directory
   * @return true if directory was parsed succesfully, false otherwise
   */
  bool RefreshList(const std::filesystem::path& dir_path);

  /**
   * @brief Refresh list to keep only files matching pattern from the text to search
   */
  void RefreshSearchList();

  /**
   * @brief Update content from active entry (decides if animation thread should run or not)
   */
  void UpdateActiveEntry();

  /**
   * @brief Select file to play based on the current song playing
   * @param pick_next Pick next or previous file to play
   * @return Filepath
   */
  util::File SelectFileToPlay(bool pick_next);

  /**
   * @brief Execute click action on active entry (may change directory or play song)
   * @return true if click action was executed, false otherwise
   */
  bool ClickOnActiveEntry();

  /* ******************************************************************************************** */
  //! Local cache for current active information
 protected:
  //! Get current directory
  const std::filesystem::path& GetCurrentDir() const { return curr_dir_; }

 private:
  std::filesystem::path curr_dir_;                                    //!< Current directory
  std::optional<std::filesystem::path> curr_playing_ = std::nullopt;  //!< Current song playing

  /* ******************************************************************************************** */
  //! Custom class for search and style

  //! Parameters for when search mode is enabled
  struct Search {
    std::string text_to_search;  //!< Text to search in file entries
    util::Files entries;  //!< List containing only files from current directory matching the text
    int selected;         //!< Entry index in files list for entry selected
    int focused;          //!< Entry index in files list for entry focused
    int position;         //!< Cursor position for text to search
  };

  //! Put together all possible styles for an entry in this component
  struct EntryStyles {
    ftxui::Decorator title;
    ftxui::Decorator prefix;
    MenuEntryOption directory;
    MenuEntryOption file;
    MenuEntryOption playing;
  };

  /* ******************************************************************************************** */
  //! Variables
 private:
  int max_columns_;  //!< Maximum number of columns (characters in a single line) available to use
  std::shared_ptr<util::FileHandler> file_handler_;  //!< Utility class to manage files (read/write)

  util::Files entries_;  //!< List containing files from current directory
  int selected_;         //!< Entry index in files list for entry selected
  int focused_;          //!< Entry index in files list for entry focused

  std::vector<ftxui::Box> boxes_;  //!< Single box for each entry in files list
  ftxui::Box box_;                 //!< Box for whole component

  std::optional<Search> mode_search_ =
      std::nullopt;  //!< Mode to render only files matching the search pattern

  TextAnimation animation_;  //!< Text animation for selected entry

  std::chrono::system_clock::time_point last_click_;  //!< Last timestamp that mouse was clicked

  //!< Style for each element inside this component
  EntryStyles styles_ = EntryStyles{
      .title = ftxui::color(ftxui::Color::White) | ftxui::bold,
      .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
      .directory = Colored(ftxui::Color::Green),
      .file = Colored(ftxui::Color::White),
      .playing = Colored(ftxui::Color::SteelBlue1),
  };

  /* ******************************************************************************************** */
  //! Friend test

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
  friend class ::ListDirectoryCtorTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_LIST_DIRECTORY_H_
