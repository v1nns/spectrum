/**
 * \file
 * \brief  Class for rendering a customized menu list
 */

#ifndef INCLUDE_VIEW_ELEMENT_INTERNAL_MENU_H_
#define INCLUDE_VIEW_ELEMENT_INTERNAL_MENU_H_

#include <iomanip>
#include <string>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/event_dispatcher.h"
#include "view/base/keybinding.h"
#include "view/element/text_animation.h"
#include "view/element/util.h"

namespace interface {
namespace internal {

/**
 * @brief Interface for customized menu list (using CRTP pattern for static polymorphism)
 */
template <typename Derived>
class Menu {
  static constexpr int kMaxIconColumns = 2;  //!< Maximum columns for Icon

  //! Parameters for search mode
  struct Search {
    std::string text_to_search = "";  //!< Text to search in file entries
    int selected = 0;                 //!< Entry index in files list for entry selected
    int focused = 0;                  //!< Entry index in files list for entry focused
    int position = 0;                 //!< Cursor position for text to search
  };

  //! Get menu implementation (derived class)
  Derived& actual() { return *static_cast<Derived*>(this); }
  Derived const& actual() const { return *static_cast<Derived const*>(this); }

  /* ******************************************************************************************** */
  //! Menu styling
 protected:
  //! Custom style for menu entry TODO: better organize this
  struct MenuEntryOption {
    ftxui::Decorator normal;
    ftxui::Decorator focused;
    ftxui::Decorator selected;
    ftxui::Decorator selected_focused;
  };

  //! Decorator for custom style to apply on menu entry
  inline MenuEntryOption Colored(ftxui::Color c) {
    using ftxui::color;
    using ftxui::Decorator;
    using ftxui::inverted;

    return MenuEntryOption{
        .normal = Decorator(color(c)),
        .focused = Decorator(color(c)) | inverted,
        .selected = Decorator(color(c)) | inverted,
        .selected_focused = Decorator(color(c)) | inverted,
    };
  }

  /* ******************************************************************************************** */
  //! Constructor/Destructor

  /**
   * @brief Construct a new Menu object (called by derived class constructor)
   * @param dispatcher Event dispatcher
   * @param force_refresh Callback function to force UI update
   */
  explicit Menu(const std::shared_ptr<EventDispatcher>& dispatcher,
                const TextAnimation::Callback& force_refresh)
      : dispatcher_{dispatcher}, animation_{TextAnimation{.cb_update = force_refresh}} {}

 public:
  /**
   * @brief Destroy Menu object
   */
  virtual ~Menu() = default;

  /* ******************************************************************************************** */
  //! Public API for Menu (must be implemented by derived class)

  /**
   * @brief Renders the element
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() { return actual().RenderImpl(); };

  /**
   * @brief Handles an event from mouse/keyboard
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) {
    if (IsSearchEnabled() && OnSearchModeEvent(event)) return true;

    if (GetSize() && OnMenuNavigation(event)) return true;

    return actual().OnEventImpl(event);
  }

  /**
   * @brief Handles an event from mouse
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event& event) {
    if (event.mouse().button == ftxui::Mouse::WheelDown ||
        event.mouse().button == ftxui::Mouse::WheelUp)
      return OnMouseWheel(event);

    if (event.mouse().button != ftxui::Mouse::Left && event.mouse().button != ftxui::Mouse::None)
      return false;

    int* selected = GetSelected();
    int* focused = GetFocused();

    bool entry_focused = false;

    for (int i = 0; i < GetSize(); ++i) {
      if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) continue;

      entry_focused = true;
      *focused = i;

      if (event.mouse().button == ftxui::Mouse::Left &&
          event.mouse().motion == ftxui::Mouse::Released) {
        LOG("Handle left click mouse event on entry=", i);
        *selected = i;

        // Check if this is a double-click event
        auto now = std::chrono::system_clock::now();
        if (now - last_click_ <= std::chrono::milliseconds(500)) return OnClick();

        last_click_ = now;
        return true;
      }
    }

    // If no entry was focused with mouse, reset index
    if (!entry_focused) *focused = *selected;

    return false;
  }

  /**
   * @brief Get total size of entries in menu
   * @return Menu entries size
   */
  int GetSize() const { return actual().GetSizeImpl(); };

  /**
   * @brief Get active entry as text
   * @return String containing active entry content
   */
  std::string GetActiveEntryAsText() const { return actual().GetActiveEntryAsTextImpl(); };

  /**
   * @brief Execute OnClick action on current entry (also known as selected)
   * @return true if event was handled, otherwise false
   */
  bool OnClick() { return actual().OnClickImpl(); }

  /* ******************************************************************************************** */
  //! Internal event handling
 private:
  //! Handle mouse wheel event
  bool OnMouseWheel(ftxui::Event& event) {
    if (!box_.Contain(event.mouse().x, event.mouse().y)) {
      return false;
    }

    bool is_wheel_up = event.mouse().button == ftxui::Mouse::WheelUp;
    LOG("Handle mouse wheel event=", is_wheel_up ? std::quoted("Up") : std::quoted("Down"));

    int size = GetSize();
    int* selected = GetSelected();
    int* focused = GetFocused();

    *selected = *focused;

    // Update indexes based on wheel event
    if (is_wheel_up) {
      (*selected)--;
      (*focused)--;
    } else {
      (*selected)++;
      (*focused)++;
    }

    *selected = clamp(*selected, 0, size - 1);
    *focused = clamp(*focused, 0, size - 1);

    // Check if must enable text animation
    UpdateActiveEntry();
    return true;
  }

  //! Handle keyboard event mapped to a menu navigation command
  bool OnMenuNavigation(const ftxui::Event& event) {
    using Keybind = keybinding::Navigation;
    bool event_handled = false;

    int size = GetSize();
    int* selected = GetSelected();
    int* focused = GetFocused();

    int old_selected = *selected;

    if (event == Keybind::ArrowUp || event == Keybind::Up)
      *selected = (*selected + size - 1) % size;
    if (event == Keybind::ArrowDown || event == Keybind::Down) *selected = (*selected + 1) % size;
    if (event == Keybind::PageUp) (*selected) -= box_.y_max - box_.y_min;
    if (event == Keybind::PageDown) (*selected) += box_.y_max - box_.y_min;
    if (event == Keybind::Home) (*selected) = 0;
    if (event == Keybind::End) (*selected) = size - 1;

    if (*selected != old_selected) {
      *selected = clamp(*selected, 0, size - 1);

      if (*selected != old_selected) {
        *focused = *selected;
        event_handled = true;

        // Check if must enable text animation
        UpdateActiveEntry();
      }
    }

    // Otherwise, user clicked on active entry
    if (event == Keybind::Return) {
      event_handled = OnClick();

      if (event_handled) ResetSearch();
    }

    LOG_IF(event_handled, "Handled menu navigation key=", std::quoted(util::EventToString(event)));
    return event_handled;
  }

  //! Handle keyboard event when search mode is enabled
  bool OnSearchModeEvent(const ftxui::Event& event) {
    using Keybind = keybinding::Navigation;

    bool event_handled = false;
    bool exit_from_search_mode = false;

    // Any alphabetic character
    if (event.is_character()) {
      search_params_->text_to_search.insert(search_params_->position, event.character());
      search_params_->position++;
      event_handled = true;
    }

    // Backspace
    if (event == Keybind::Backspace && !(search_params_->text_to_search.empty())) {
      if (search_params_->position > 0) {
        search_params_->text_to_search.erase(search_params_->position - 1, 1);
        search_params_->position--;
      }
      event_handled = true;
    }

    // Ctrl + Backspace
    if (event == Keybind::CtrlBackspace || event == Keybind::CtrlBackspaceReverse) {
      search_params_->text_to_search.clear();
      search_params_->position = 0;
      event_handled = true;
    }

    // Arrow left
    if (event == Keybind::ArrowLeft) {
      if (search_params_->position > 0) search_params_->position--;
      event_handled = true;
    }

    // Arrow right
    if (event == Keybind::ArrowRight) {
      if (auto size = (int)search_params_->text_to_search.size(); search_params_->position < size)
        search_params_->position++;
      event_handled = true;
    }

    // Quit search mode
    if (event == Keybind::Escape) {
      LOG("Exit from search mode in menu");
      ResetSearch();

      event_handled = true;
      exit_from_search_mode = true;
    }

    if (event_handled && !exit_from_search_mode) {
      RefreshSearchList();
    }

    return event_handled;
  }

  /* ******************************************************************************************** */
  //! Public setters
 public:
  /**
   * @brief Set maximum value of columns available to render text
   * @param max_columns Number of columns
   */
  void SetMaxColumns(int max_columns) { max_columns_ = max_columns; }

  /**
   * @brief Save entries internally to render it as a menu
   * @param entries New list of entries for menu
   */
  template <typename T>
  void SetEntries(const T& entries) {
    LOG("Set a new list of entries");
    actual().SetEntriesImpl(entries);

    ResetState();
    Clamp();
  }

  /**
   * @brief Emplace a new entry to menu list
   * @param entry New entry for menu
   */
  template <typename T>
  void Emplace(const T& entry) {
    LOG("Emplace a new entry to list");
    actual().EmplaceImpl(entry);
    Clamp();
  }

  /**
   * @brief Reset internal UI state
   * @param new_index Index to force select/focus state
   */
  void ResetState(int new_index = 0) {
    LOG("Reset state with new index=", new_index);
    selected_ = new_index;
    focused_ = new_index;

    int size = GetSize();

    selected_ = clamp(selected_, 0, size - 1);
    focused_ = clamp(focused_, 0, size - 1);
    // ResetSearch();
  }

  /* ******************************************************************************************** */
  //! Public getters

  //! Getter for active entry (current selected) from derived class
  decltype(auto) GetActiveEntry() { return actual().GetActiveEntryImpl(); }

  //! Getter for all entries from derived class
  decltype(auto) GetEntries() { return actual().GetEntriesImpl(); }

  /* ******************************************************************************************** */
  //! Protected getters for use in derived class
 protected:
  //! Getter for element box
  ftxui::Box& GetBox() { return box_; }

  //! Getter for entries box
  std::vector<ftxui::Box>& GetBoxes() { return boxes_; }

  //! Getter for selected index
  int* GetSelected() { return search_params_.has_value() ? &search_params_->selected : &selected_; }

  //! Getter for selected index (const)
  int GetSelected() const {
    return search_params_.has_value() ? search_params_->selected : selected_;
  }

  //! Getter for focused index
  int* GetFocused() { return search_params_.has_value() ? &search_params_->focused : &focused_; }

  //! Getter for focused index (const)
  int GetFocused() const { return search_params_.has_value() ? search_params_->focused : focused_; }

  //! Getter for maximum columns
  int GetMaxColumns() const { return max_columns_; }

  //! Check if text animation is enabled
  bool IsAnimationRunning() const { return animation_.enabled; }

  //! Getter for text from animation effect
  std::string GetTextFromAnimation() const { return animation_.text; }

  /* ******************************************************************************************** */
  //! Highlight entry
 public:
  /**
   * @brief Set entry to be highlighted
   * @param entry Menu entry to get highlight
   */
  template <typename T>
  void SetEntryHighlighted(const T& entry) {
    actual().SetEntryHighlightedImpl(entry);
  }

  /**
   * @brief Reset highlighted entry
   */
  void ResetHighlight() { actual().ResetHighlightImpl(); }

  /* ******************************************************************************************** */
  //! Search mode
 protected:
  //! Getter for search parameters
  const std::optional<Search>& GetSearch() const { return search_params_; }

  //! Check if search mode enabled
  bool IsSearchEnabled() const { return search_params_.has_value(); }

 public:
  /**
   * @brief Enable search mode
   */
  void EnableSearch() {
    LOG("Enable search mode");
    search_params_ = Search{.selected = selected_, .focused = focused_};
    actual().FilterEntriesBy(search_params_->text_to_search);

    // Send user action to controller, disable global events
    auto dispatcher = dispatcher_.lock();
    if (!dispatcher) return;

    auto event_global = interface::CustomEvent::DisableGlobalEvent();
    dispatcher->SendEvent(event_global);
  }

  /**
   * @brief Reset search mode (if enabled)
   */
  void ResetSearch() {
    if (!search_params_.has_value()) return;

    LOG("Reset search mode");

    // Send user action to controller, enable global events again
    if (auto dispatcher = dispatcher_.lock(); dispatcher) {
      auto event_global = interface::CustomEvent::EnableGlobalEvent();
      dispatcher->SendEvent(event_global);
    }

    // Reset search parameters and entries
    search_params_.reset();
    actual().ResetSearchImpl();

    Clamp();

    // Update active entry (to enable/disable text animation)
    UpdateActiveEntry();
  }

 private:
  //! Refresh list to keep only files matching pattern from the text to search
  void RefreshSearchList() {
    LOG("Refresh list on search mode, text=", std::quoted(search_params_->text_to_search));

    // Trigger callback (from derived class) to filter entries
    actual().FilterEntriesBy(search_params_->text_to_search);

    Clamp();

    // Check if must enable text animation
    UpdateActiveEntry();
  }

  /* ******************************************************************************************** */
  //! Internal handling

  //! Clamp both selected and focused indexes
  void Clamp() {
    int size = GetSize();
    boxes_.resize(size);

    int* selected = IsSearchEnabled() ? &search_params_->selected : &selected_;
    int* focused = IsSearchEnabled() ? &search_params_->focused : &focused_;

    *selected = clamp(*selected, 0, size - 1);
    *focused = clamp(*focused, 0, size - 1);
  }

  //! Update content from active entry (decides if text_animation should run or not)
  void UpdateActiveEntry() {
    // Stop animation thread
    animation_.Stop();

    if (!max_columns_ || !GetSize()) return;

    std::string text = GetActiveEntryAsText();
    int count_chars = (int)text.length() + kMaxIconColumns;

    // Start animation thread
    if (count_chars > max_columns_) animation_.Start(text);
  }

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  int max_columns_ = 0;  //!< Maximum value of columns available to render text

  ftxui::Box box_;                 //!< Box to control if mouse cursor is over the menu
  std::vector<ftxui::Box> boxes_;  //!< Single box for each entry in files list

  int selected_ = 0;  //!< Index in list for selected entry
  int focused_ = 0;   //!< Index in list for focused entry

  std::chrono::system_clock::time_point last_click_;  //!< Last timestamp that mouse was clicked

  //!< Mode to render only files matching the search pattern
  std::optional<Search> search_params_ = std::nullopt;

  TextAnimation animation_;  //!< Text animation for selected entry
};

}  // namespace internal
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_INTERNAL_MENU_H_
