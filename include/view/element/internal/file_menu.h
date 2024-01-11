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
#include "view/element/internal/menu.h"
#include "view/element/text_animation.h"

#ifdef ENABLE_TESTS
namespace {
class SidebarTest;
}
#endif

namespace interface {

namespace internal {
class FileMenu : public Menu<FileMenu> {
 private:
  friend class Menu;

  //! Put together all possible styles for an entry in this component
  struct Style {
    ftxui::Decorator prefix;
    MenuEntryOption directory;
    MenuEntryOption file;
    MenuEntryOption playing;
  };

 public:
  /**
   * @brief Callback triggered by action executed on current active menu item
   * @param active Current menu item active
   * @return true if click action was executed, false otherwise
   */
  using Callback = std::function<bool(const std::optional<util::File>& active)>;

  /**
   * @brief Construct a new FileMenu object
   * @param force_refresh Callback function to update UI
   * @param on_click Callback function for click event on active entry
   */
  explicit FileMenu(const TextAnimation::Callback& force_refresh, const Callback& on_click);

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
  int GetSizeImpl() const {
    int size = IsSearchEnabled() ? (int)filtered_entries_->size() : (int)entries_.size();
    return size;
  }

  //! Getter for active entry as text (focused/selected)
  std::string GetActiveEntryAsTextImpl() const {
    auto active = GetActiveEntryImpl();
    return active.has_value() ? active->filename().string() : "";
  }

  //! Execute action on the active entry
  bool OnClickImpl();

  //! While on search mode, filter all entries to keep only those matching the given text
  void FilterEntriesBy(const std::string& text);

  /* ******************************************************************************************** */
  //! Setters and getters

  //! Save entries internally to render it as a menu
  void SetEntriesImpl(const util::Files& entries) { entries_ = entries; }

  //! Getter for entries
  util::Files GetEntriesImpl() const { return IsSearchEnabled() ? *filtered_entries_ : entries_; }

  //! Emplace a new entry
  void EmplaceImpl(const util::File& entry) { entries_.emplace_back(entry); }

  //! Set entry to be highlighted
  void SetEntryHighlightedImpl(const util::File& entry);

  //! Reset highlighted entry
  void ResetHighlightImpl() { highlighted_.reset(); };

  //! Getter for active entry (focused/selected)
  std::optional<util::File> GetActiveEntryImpl() const {
    // Empty list
    if (!GetSize()) return std::nullopt;

    // Get active entry
    int index = GetSelected();
    return IsSearchEnabled() ? filtered_entries_->at(index) : entries_.at(index);
  }

  //! Reset search mode (if enabled) and highlight the given entry
  void ResetSearchImpl() { filtered_entries_.reset(); }

  /* ******************************************************************************************** */
  //! Variables
 private:
  util::Files entries_;  //!< List containing files from current directory

  //!< List containing only files matching the text from search
  std::optional<util::Files> filtered_entries_ = std::nullopt;

  std::optional<util::File> highlighted_ = std::nullopt;  //!< Entry highlighted by owner

  Callback on_click_;  // TODO: DOC

  //!< Style for each element inside this component
  Style style_ = Style{
      .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
      .directory = Colored(ftxui::Color::Green),
      .file = Colored(ftxui::Color::White),
      .playing = Colored(ftxui::Color::SteelBlue1),
  };

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
#endif
};

}  // namespace internal
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_INTERNAL_FILE_MENU_H_
