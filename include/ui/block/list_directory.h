/**
 * \file
 * \brief  Class for block containing file list
 */

#ifndef INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_
#define INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"  // for Radiobox, Horizontal, Menu, Renderer, Tab
#include "ftxui/component/event.hpp"      // for Event
#include "ftxui/dom/elements.hpp"  // for operator|, Element, reflect, text, nothing, select, vbox, Elements, focus
#include "ftxui/dom/node.hpp"

namespace interface {

using namespace ftxui;

/**
 * @brief Represent a single file entry
 *
 */
struct File {
  std::string path;
  bool is_dir;
};

//! For better readability
using Files = std::vector<File>;

/**
 * @brief Class for List files in directory block
 */
class ListDirectory : public ComponentBase {
 public:
  ListDirectory();

  Element Render() override;
  bool OnEvent(Event event) override;

  /* ******************************************************************************************** */

  bool OnMouseEvent(Event event);
  bool OnMouseWheel(Event event);

  /* ******************************************************************************************** */
 private:
  bool Focusable() const final { return entries_.size(); }
  int size() const { return entries_.size(); }

  void Clamp();

 private:
  /**
   * @brief Refresh list with files from new or current directory
   *
   * @param dir_path Full path to directory
   */
  void RefreshList(const std::filesystem::path& dir_path);

  /* ******************************************************************************************** */
 private:
  std::filesystem::path curr_dir_;  //!< Current directory
  Files entries_;                   //!< List containing files from current directory
  int selected_, focused_;

  std::vector<Box> boxes_;
  Box box_;
};

}  // namespace interface
#endif  // INCLUDE_UI_BLOCK_LIST_DIRECTORY_H_