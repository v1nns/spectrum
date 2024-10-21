/**
 * \file
 * \brief  Class for rendering a customized menu containing text entries
 */

#ifndef INCLUDE_VIEW_ELEMENT_INTERNAL_TEXT_MENU_H_
#define INCLUDE_VIEW_ELEMENT_INTERNAL_TEXT_MENU_H_

#include <string>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "view/element/internal/base_menu.h"
#include "view/element/text_animation.h"

#ifdef ENABLE_TESTS
namespace {
class SidebarTest;
}
#endif

namespace interface {

namespace internal {
class TextMenu : public BaseMenu<TextMenu> {
  friend class BaseMenu;

  //! Put together all possible styles for an entry in this component
  struct Style {
    ftxui::Decorator prefix;
    MenuEntryOption entry;
  };

 public:
  //!< Callback definition for function that will be triggered when a menu entry is clicked/pressed
  using Callback = Callback<std::string>;

  /**
   * @brief Construct a new TextMenu object
   * @param dispatcher Event dispatcher
   * @param force_refresh Callback function to update UI
   * @param on_click Callback function for click event on active entry
   */
  explicit TextMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                    const TextAnimation::Callback& force_refresh, const Callback& on_click);

  /**
   * @brief Destroy Menu object
   */
  ~TextMenu() override = default;

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
  void SetEntriesImpl(const std::vector<std::string>& entries);

  //! Getter for entries
  std::vector<std::string> GetEntriesImpl() const {
    return IsSearchEnabled() ? *filtered_entries_ : entries_;
  }

  //! Emplace a new entry
  void EmplaceImpl(const std::string& entry) {
    LOG("Emplace a new entry to list");
    entries_.emplace_back(entry);
  }

  //! Set entry to be highlighted
  void SetEntryHighlightedImpl(const std::string&) {}

  //! Reset highlighted entry
  void ResetHighlightImpl() {}

  //! Getter for active entry (focused/selected)
  std::optional<std::string> GetActiveEntryImpl() const;

  //! Reset search mode (if enabled) and highlight the given entry
  void ResetSearchImpl() { filtered_entries_.reset(); }

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::vector<std::string> entries_;  //!< List containing text entries

  //!< List containing only entries matching the text from search
  std::optional<std::vector<std::string>> filtered_entries_ = std::nullopt;

  Callback on_click_;  //!< Callback function to trigger when menu entry is clicked/pressed

  //!< Style for each element inside this component
  Style style_ = Style{
      .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
      .entry = Colored(ftxui::Color::White),
  };

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
#endif
};

}  // namespace internal
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_INTERNAL_TEXT_MENU_H_
