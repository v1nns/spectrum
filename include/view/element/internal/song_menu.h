/**
 * \file
 * \brief  Class for rendering a customized menu containing text entries
 */

#ifndef INCLUDE_VIEW_ELEMENT_INTERNAL_SONG_MENU_H_
#define INCLUDE_VIEW_ELEMENT_INTERNAL_SONG_MENU_H_

#include <deque>
#include <string>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/song.h"
#include "view/element/internal/base_menu.h"
#include "view/element/text_animation.h"

#ifdef ENABLE_TESTS
namespace {
class SidebarTest;
}
#endif

namespace interface::internal {

class SongMenu : public BaseMenu<SongMenu> {
  friend class BaseMenu;

  //! Put together all possible styles for an entry in this component
  struct Style {
    ftxui::Decorator prefix;
    MenuEntryOption entry;
  };

  //! Define a custom value for maximum number of columns used as icon
  static constexpr int GetMaxColumnsForIconImpl() { return -1; }

 public:
  //!< Callback definition for function that will be triggered when a menu entry is clicked/pressed
  using Callback = Callback<model::Song>;

  /**
   * @brief Construct a new SongMenu object
   * @param dispatcher Event dispatcher
   * @param force_refresh Callback function to update UI
   * @param on_click Callback function for click event on active entry
   */
  explicit SongMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                    const TextAnimation::Callback& force_refresh, const Callback& on_click);

  /**
   * @brief Destroy Menu object
   */
  ~SongMenu() override = default;

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
  //! Setters and getters

  //! Save entries internally to render it as a menu
  void SetEntriesImpl(const std::deque<model::Song>& entries);

  //! Getter for entries
  std::deque<model::Song> GetEntriesImpl() const {
    return IsSearchEnabled() ? *filtered_entries_ : entries_;
  }

  //! Emplace a new entry
  void EmplaceImpl(const model::Song& entry);

  //! Erase an existing entry
  void EraseImpl(const model::Song& entry);

  //! Set entry to be highlighted
  void SetEntryHighlightedImpl(const std::string&) {}

  //! Reset highlighted entry
  void ResetHighlightImpl() {}

  //! Getter for active entry (focused/selected)
  std::optional<model::Song> GetActiveEntryImpl() const;

  //! Reset search mode (if enabled) and highlight the given entry
  void ResetSearchImpl() { filtered_entries_.reset(); }

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::deque<model::Song> entries_;  //!< List containing song entries

  //!< List containing only entries matching the text from search
  std::optional<std::deque<model::Song>> filtered_entries_ = std::nullopt;

  Callback on_click_;  //!< Callback function to trigger when menu entry is clicked/pressed

  //!< Style for each element inside this component
  Style style_ = Style{
      .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
      .entry = Colored(ftxui::Color::Grey11),
  };

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
#endif
};

}  // namespace interface::internal
#endif  // INCLUDE_VIEW_ELEMENT_INTERNAL_SONG_MENU_H_
