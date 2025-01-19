/**
 * \file
 * \brief  Class for rendering a customized menu list
 */

#ifndef INCLUDE_VIEW_ELEMENT_INTERNAL_BASE_MENU_H_
#define INCLUDE_VIEW_ELEMENT_INTERNAL_BASE_MENU_H_

#include <iomanip>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/element.h"
#include "view/base/event_dispatcher.h"
#include "view/base/keybinding.h"
#include "view/element/text_animation.h"
#include "view/element/util.h"

namespace interface {

namespace menu {

/**
 * @brief Theme alternatives for Menu component
 */
enum class Style {
  Default = 3000,
  Alternative = 3001,
};

}  // namespace menu

namespace internal {

/**
 * @brief Interface for customized menu list (using CRTP pattern for static polymorphism)
 */
template <typename Derived>
class BaseMenu : public Element {
  static constexpr int kMaxIconColumns = 2;  //!< Maximum columns for Icon

  //! Parameters for search mode
  struct Search {
    std::string text_to_search = "";  //!< Text to search in file entries
    int selected_index = 0;           //!< Entry index in files list for entry selected
    int focused_index = 0;            //!< Entry index in files list for entry focused
    int position = 0;                 //!< Cursor position for text to search
  };

 public:
  //! Get menu implementation (derived class)
  Derived& actual() { return *static_cast<Derived*>(this); }
  Derived const& actual() const { return *static_cast<Derived const*>(this); }

  /* ******************************************************************************************** */
  //! Menu styling and callback definition
 protected:
  //! Custom style for menu entry TODO: better organize this
  struct MenuEntryOption {
    ftxui::Decorator normal;
    ftxui::Decorator focused;
    ftxui::Decorator selected;
    ftxui::Decorator selected_focused;
  };

  //! Decorator for custom style to apply on menu entry
  inline MenuEntryOption Colored(const ftxui::Color& c, bool is_bold = false) {
    using ftxui::bold;
    using ftxui::color;
    using ftxui::Decorator;
    using ftxui::inverted;
    using ftxui::nothing;

    return MenuEntryOption{
        .normal = Decorator(color(c)) | (is_bold ? bold : nothing),
        .focused = Decorator(color(c)) | (is_bold ? bold : nothing) | inverted,
        .selected = Decorator(color(c)) | (is_bold ? bold : nothing) | inverted,
        .selected_focused = Decorator(color(c)) | (is_bold ? bold : nothing) | inverted,
    };
  }

  /**
   * @brief Callback triggered by action executed on current active menu item
   * @param active Current menu item active
   * @return true if click action was executed, false otherwise
   */
  template <typename T>
  using Callback = std::function<bool(const std::optional<T>& active)>;

  /* ******************************************************************************************** */
  //! Constructor/Destructor

  /**
   * @brief Construct a new Menu object (called by derived class constructor)
   * @param dispatcher Event dispatcher
   * @param force_refresh Callback function to force UI update
   */
  explicit BaseMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                    const TextAnimation::Callback& force_refresh)
      : Element(), dispatcher_{dispatcher}, animation_{TextAnimation{.cb_update = force_refresh}} {}

 public:
  /**
   * @brief Destroy Menu object
   */
  ~BaseMenu() override = default;

  /* ******************************************************************************************** */
  //! Public API for Menu (must be implemented by derived class)

  /**
   * @brief Renders the element
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override { return actual().RenderImpl(); };

  /**
   * @brief Handles an event from keyboard
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) override {
    if (IsSearchEnabled() && OnSearchModeEvent(event)) return true;

    if (GetSize() && OnMenuNavigation(event)) return true;

    return actual().OnEventImpl(event);
  }

  /**
   * @brief Handles a click event from mouse
   * @param event Received event from screen
   */
  void HandleClick(ftxui::Event& event) override { UpdateFocusedEntry(event); }

  /**
   * @brief Handles a double click event from mouse
   * @param event Received event from screen
   */
  void HandleDoubleClick(ftxui::Event& event) override {
    // TODO: update animated entry based also on mouse focus
    UpdateFocusedEntry(event);
  }

  /**
   * @brief Handles a hover event from mouse
   * @param event Received event from screen
   */
  void HandleHover(ftxui::Event& event) override { UpdateFocusedEntry(event, false); }

  /**
   * @brief Handles a mouse wheel event
   * @param button Received button from mouse wheel event
   */
  void HandleWheel(const ftxui::Mouse::Button& button) override {
    bool is_wheel_up = button == ftxui::Mouse::WheelUp;
    LOG_T("Handle mouse wheel event=", is_wheel_up ? "Up" : "Down");

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
  //! Focus a menu entry based on a mouse event
  void UpdateFocusedEntry(ftxui::Event& event, bool click = true) {
    int* selected = GetSelected();
    int* focused = GetFocused();

    bool entry_focused = false;

    for (int i = 0; i < GetSize(); ++i) {
      if (!boxes_[i].Contain(event.mouse().x, event.mouse().y)) continue;

      LOG_T("Handle double left click mouse event on entry=", i);
      entry_focused = true;
      *focused = i;
      *selected = i;

      if (click) OnClick();
      break;
    }

    // If no entry was focused with mouse, reset index
    if (!entry_focused) *focused = *selected;
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
    if (event == Keybind::PageUp) (*selected) -= Box().y_max - Box().y_min;
    if (event == Keybind::PageDown) (*selected) += Box().y_max - Box().y_min;
    if (event == Keybind::Home) (*selected) = 0;
    if (event == Keybind::End) (*selected) = size - 1;

    if (*selected != old_selected) {
      *selected = clamp(*selected, 0, size - 1);

      if (*selected != old_selected) {
        LOG_T("Handled menu navigation key=", util::EventToString(event));
        *focused = *selected;
        event_handled = true;

        // Check if must enable text animation
        UpdateActiveEntry();
      }
    }

    // Otherwise, user clicked on active entry
    if (event == Keybind::Return) {
      event_handled = OnClick();

      LOG_T_IF(event_handled, "Handled Return key");

      // Always reset search mode
      ResetSearch();
    }

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
      LOG_T("Exit from search mode in menu");
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
   * NOTE: without setting this, animation will not work as intended
   */
  void SetMaxColumns(int max_columns) { max_columns_ = max_columns; }

  /**
   * @brief Save entries internally to render it as a menu
   * @param entries New list of entries for menu
   */
  template <typename T>
  void SetEntries(const T& entries) {
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
    actual().EmplaceImpl(entry);
    Clamp();
  }

  /**
   * @brief Erase an existing entry from menu list
   * @param entry Menu entry
   */
  template <typename T>
  void Erase(const T& entry) {
    actual().EraseImpl(entry);
    Clamp();
    UpdateActiveEntry();
  }

  /**
   * @brief Reset internal UI state
   * @param index Index to force select/focus state
   */
  void ResetState(int index = 0) {
    LOG_T("Reset state with new index=", index);
    selected_index_ = index;
    focused_index_ = index;

    int size = GetSize();

    selected_index_ = clamp(selected_index_, 0, size - 1);
    focused_index_ = clamp(focused_index_, 0, size - 1);
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
  //! Getter for event dispatcher
  std::shared_ptr<EventDispatcher> GetDispatcher() const { return dispatcher_.lock(); }

  //! Getter for entries box
  std::vector<ftxui::Box>& GetBoxes() { return boxes_; }

  //! Getter for selected index
  int* GetSelected() {
    return search_params_.has_value() ? &search_params_->selected_index : &selected_index_;
  }

  //! Getter for selected index (const)
  int GetSelected() const {
    return search_params_.has_value() ? search_params_->selected_index : selected_index_;
  }

  //! Getter for focused index
  int* GetFocused() {
    return search_params_.has_value() ? &search_params_->focused_index : &focused_index_;
  }

  //! Getter for focused index (const)
  int GetFocused() const {
    return search_params_.has_value() ? search_params_->focused_index : focused_index_;
  }

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

  //! Render UI element for search
  ftxui::Element RenderSearch() const {
    if (!IsSearchEnabled()) {
      ERROR_T("Internal error");
      return ftxui::text("");
    }

    ftxui::InputOption opt{.cursor_position = search_params_->position};

    return ftxui::hbox({
        ftxui::text("Search:") | ftxui::color(ftxui::Color::White),
        ftxui::Input(search_params_->text_to_search, " ", &opt)->Render() | ftxui::flex,
    });
  }

 public:
  /**
   * @brief Enable search mode
   */
  void EnableSearch() {
    LOG_T("Enable search mode");
    search_params_ = Search{.selected_index = selected_index_, .focused_index = focused_index_};
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
    LOG_T("Reset search mode");

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
    LOG_T("Refresh list on search mode, text=", std::quoted(search_params_->text_to_search));

    // Trigger callback (from derived class) to filter entries
    actual().FilterEntriesBy(search_params_->text_to_search);

    // Resize internal structures and check if must run text animation on selected entry
    Clamp();
    UpdateActiveEntry();
  }

  /* ******************************************************************************************** */
  //! Internal handling

  //! Notify when focus state has changed
  void OnFocusChanged() override {
    if (IsFocused()) {
      // Check if can execute text animation on active entry
      UpdateActiveEntry();
    } else if (IsAnimationRunning()) {
      animation_.Stop();
    }
  }

  /* ******************************************************************************************** */
  //! Protected API to also use by derived classes
 protected:
  //! Clamp both selected and focused indexes
  void Clamp() {
    int size = GetSize();
    boxes_.resize(size);

    int* selected = IsSearchEnabled() ? &search_params_->selected_index : &selected_index_;
    int* focused = IsSearchEnabled() ? &search_params_->focused_index : &focused_index_;

    *selected = clamp(*selected, 0, size - 1);
    *focused = clamp(*focused, 0, size - 1);
  }

  //! Update content from active entry (decides if text_animation should run or not)
  void UpdateActiveEntry() {
    // Stop animation thread
    animation_.Stop();

    // Do not attempt to start animation thread if:
    //  - max_columns was not set by parent element
    //  - inner menu list is empty
    if (!max_columns_ || !GetSize()) return;

    // Get active entry and count char length
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

  std::vector<ftxui::Box> boxes_;  //!< Single box for each entry in files list

  int selected_index_ = 0;  //!< Index in list for selected entry
  int focused_index_ = 0;   //!< Index in list for focused entry

  //!< Mode to render only files matching the search pattern
  std::optional<Search> search_params_ = std::nullopt;

  TextAnimation animation_;  //!< Text animation for selected entry
};

}  // namespace internal
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_INTERNAL_BASE_MENU_H_
