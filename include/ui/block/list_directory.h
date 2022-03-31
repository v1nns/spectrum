/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_
#define INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_

#include <filesystem>  // for path
#include <optional>    // for optional
#include <string>      // for string, basic_string
#include <vector>      // for vector

#include "ftxui/component/component_base.hpp"     // for ComponentBase
#include "ftxui/component/component_options.hpp"  // for MenuEntryOption
#include "ftxui/dom/elements.hpp"                 // for Element
#include "ftxui/screen/box.hpp"                   // for Box
#include "ui/base/block.h"

namespace ftxui {
struct Event;
}  // namespace ftxui

namespace interface {

using namespace ftxui;

//! Represent a single file entry
struct File {
  std::string path;
  bool is_dir;
};

//! For better readability
using Files = std::vector<File>;

/* ********************************************************************************************** */

//! Parameters for when search mode is enabled
struct Search {
  std::string text_to_search;  //!< Text to search in file entries
  Files entries;          //!< List containing only files from current directory matching the text
  int selected, focused;  //!< Entry indexes in files list
};

/* ********************************************************************************************** */

/**
 * @brief Component to list files from given directory
 */
class ListDirectory : public Block {
 public:
  /**
   * @brief Construct a new List Directory object
   */
  ListDirectory(std::shared_ptr<Dispatcher> d, const std::string& optional_path = "");

  /**
   * @brief Destroy the List Directory object
   */
  virtual ~ListDirectory() = default;

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
  //! Handle mouse event
  bool OnMouseEvent(Event event);

  //! Handle mouse wheel event
  bool OnMouseWheel(Event event);

  //! Handle keyboard event mapped to a menu navigation command
  bool OnMenuNavigation(Event event);

  //! Handle keyboard event when search mode is enabled
  bool OnSearchModeEvent(Event event);

  /* ******************************************************************************************** */
 private:
  //! Getter for entries size
  int Size() const { return mode_search_ ? mode_search_->entries.size() : entries_.size(); }
  //! Getter for selected index
  int* GetSelected() { return mode_search_ ? &mode_search_->selected : &selected_; }
  //! Getter for focused index
  int* GetFocused() { return mode_search_ ? &mode_search_->focused : &focused_; }
  //! Getter for entry at informed index
  File& GetEntry(int i) { return mode_search_ ? mode_search_->entries.at(i) : entries_.at(i); }
  //! Getter for active entry (focused/selected)
  File* GetActiveEntry() {
    if (Size() == 0) return nullptr;

    return mode_search_ ? &mode_search_->entries.at(mode_search_->selected)
                        : &entries_.at(selected_);
  }

  //! Clamp both selected and focused indexes
  void Clamp();

  //! Getter for Title (for testing purposes, may be overriden)
  virtual std::string GetTitle();

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Refresh list with files from the given directory path
   * @param dir_path Full path to directory
   */
  void RefreshList(const std::filesystem::path& dir_path);

  /**
   * @brief Refresh list to contain only files matching pattern from the text to search
   */
  void RefreshSearchList();

  /* ******************************************************************************************** */
 protected:
  std::filesystem::path curr_dir_;  //!< Current directory

  /* ******************************************************************************************** */
 private:
  Files entries_;           //!< List containing files from current directory
  int selected_, focused_;  //!< Entry indexes in files list

  std::vector<Box> boxes_;  //!< Single box for each entry in files list
  Box box_;                 //!< Box for whole component

  std::optional<Search> mode_search_;  //!< Mode to render only files matching the search pattern

  //! Put together all possible styles for an entry in this component
  struct EntryStyles {
    MenuEntryOption directory;
    MenuEntryOption file;
  };

  EntryStyles styles_;  //!< Style for each possible type of entry on menu
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_