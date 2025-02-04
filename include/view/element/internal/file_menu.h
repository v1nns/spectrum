/**
 * \file
 * \brief  Class for rendering a customized menu containing file entries
 */

#ifndef INCLUDE_VIEW_ELEMENT_INTERNAL_FILE_MENU_H_
#define INCLUDE_VIEW_ELEMENT_INTERNAL_FILE_MENU_H_

#include <string>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/file_handler.h"
#include "view/element/internal/base_menu.h"
#include "view/element/text_animation.h"

#ifdef ENABLE_TESTS
namespace {
class SidebarTest;
}
#endif

namespace interface::internal {

class FileMenu : public BaseMenu<FileMenu> {
  friend class BaseMenu;

  //! Put together all possible styles for an entry in this component
  struct Style {
    ftxui::Decorator prefix;
    MenuEntryOption directory;
    MenuEntryOption file;
    MenuEntryOption playing;
  };

  //! Define a custom value for maximum number of columns used as icon
  static constexpr int GetMaxColumnsForIconImpl() { return -1; }

 public:
  //!< Callback definition for function that will be triggered when a menu entry is clicked/pressed
  using Callback = Callback<util::File>;

  /**
   * @brief Construct a new FileMenu object
   * @param dispatcher Event dispatcher
   * @param file_handler Utility handler to manage any file operation
   * @param force_refresh Callback function to update UI
   * @param on_click Callback function for click event on active entry
   * @param optional_path Custom directory path to fill initial list of files
   */
  explicit FileMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                    const std::shared_ptr<util::FileHandler>& file_handler,
                    const TextAnimation::Callback& force_refresh, const Callback& on_click,
                    const menu::Style& style, const std::string& optional_path);

  /**
   * @brief Destroy Menu object
   */
  ~FileMenu() override = default;

  /* ******************************************************************************************** */
  //! Mandatory API implementation
 private:
  //! Renders the element
  ftxui::Element RenderImpl();

  //! Handles an event (from mouse/keyboard)
  bool OnEventImpl(const ftxui::Event& event);

  //! Getter for menu entries size
  int GetSizeImpl() const;

  //! Getter for active entry as text (focused/selected)
  std::string GetActiveEntryAsTextImpl() const;

  //! Execute action on the active entry
  bool OnClickImpl();

  //! While on search mode, filter all entries to keep only those matching the given text
  void FilterEntriesBy(const std::string& text);

  /* ******************************************************************************************** */
  //! Derived specialization

  /**
   * @brief Compose directory path to list files from (based on given path)
   * @param optional_path Path to list files
   * @return path Directory path
   */
  std::filesystem::path ComposeDirectoryPath(const std::string& optional_path);

 public:
  /**
   * @brief Refresh list with all files from the given directory path
   * @param dir_path Full path to directory
   * @return true if directory was parsed succesfully, false otherwise
   */
  bool RefreshList(const std::filesystem::path& dir_path);

  //! Get current directory
  const std::filesystem::path& GetCurrentDir() const { return curr_dir_; }

  /* ******************************************************************************************** */
  //! Setters and getters
 private:
  //! Getter for Title (current working directory)
  std::string GetTitle() const;

  //! Save entries internally to render it as a menu
  void SetEntriesImpl(const util::Files& entries);

  //! Getter for entries
  util::Files GetEntriesImpl() const { return IsSearchEnabled() ? *filtered_entries_ : entries_; }

  //! Emplace a new entry
  void EmplaceImpl(const util::File& entry);

  //! Erase an existing entry
  void EraseImpl(const util::File& entry);

  //! Set entry to be highlighted
  void SetEntryHighlightedImpl(const util::File& entry);

  //! Reset highlighted entry
  void ResetHighlightImpl() { highlighted_.reset(); };

  //! Getter for active entry (focused/selected)
  std::optional<util::File> GetActiveEntryImpl() const;

  //! Reset search mode (if enabled) and highlight the given entry
  void ResetSearchImpl() { filtered_entries_.reset(); }

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::filesystem::path curr_dir_;  //!< Current directory
  util::Files entries_;             //!< List containing files from current directory

  //!< List containing only files matching the text from search
  std::optional<util::Files> filtered_entries_ = std::nullopt;

  std::optional<util::File> highlighted_ = std::nullopt;  //!< Entry highlighted by owner

  Callback on_click_;  //!< Callback function to trigger when menu entry is clicked/pressed

  std::shared_ptr<util::FileHandler> file_handler_;  //!< Utility class to manage files (read/write)

  Style style_;  //!< Style for each element inside this component

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
#endif
};

}  // namespace interface::internal
#endif  // INCLUDE_VIEW_ELEMENT_INTERNAL_FILE_MENU_H_
