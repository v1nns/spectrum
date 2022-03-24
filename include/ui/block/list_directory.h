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

#include "ftxui/component/captured_mouse.hpp"     // for ftxui
#include "ftxui/component/component_base.hpp"     // for ComponentBase
#include "ftxui/component/component_options.hpp"  // for MenuEntryOption
#include "ftxui/dom/elements.hpp"                 // for Element
#include "ftxui/screen/box.hpp"                   // for Box

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

/**
 * @brief Class for List files in directory block
 */
class ListDirectory : public ComponentBase {
 public:
  ListDirectory();

  // TODO: document
  Element Render() override;
  bool OnEvent(Event event) override;

  /* ******************************************************************************************** */

  bool OnMouseEvent(Event event);
  bool OnMouseWheel(Event event);

  bool OnMenuNavigation(Event event);

  bool OnSearchModeEvent(Event event);

  /* ******************************************************************************************** */
 private:
  int* GetSelected() { return mode_search_ ? &mode_search_->selected : &selected_; }
  int* GetFocused() { return mode_search_ ? &mode_search_->focused : &focused_; }
  File& GetEntry(int i) { return mode_search_ ? mode_search_->entries[i] : entries_[i]; }

  int Size() const { return mode_search_ ? mode_search_->entries.size() : entries_.size(); }

  void Clamp();

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Refresh list with files from new or current directory
   *
   * @param dir_path Full path to directory
   */
  void RefreshList(const std::filesystem::path& dir_path);

  /**
   * @brief Refresh list to contain only files matching pattern from the text to search
   */
  void RefreshSearchList();

  /* ******************************************************************************************** */
 private:
  std::filesystem::path curr_dir_;  //!< Current directory
  Files entries_;                   //!< List containing files from current directory
  int selected_, focused_;          //!< Entry indexes in files list

  MenuEntryOption style_dir_, style_file_;  //!< Style for each possible type of entry on menu

  std::vector<Box> boxes_;  //!< Single box for each entry in files list
  Box box_;                 //!< Box for whole files list

  std::optional<Search> mode_search_;  //!< Mode to render only files matching the search pattern
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_