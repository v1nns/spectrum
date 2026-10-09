/**
 * \file
 * \brief  Base class for rendering a picker (list of entries to choose one from)
 */

#ifndef INCLUDE_VIEW_ELEMENT_PICKER_H_
#define INCLUDE_VIEW_ELEMENT_PICKER_H_

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/box.hpp"
#include "view/base/element.h"
#include "view/base/keybinding.h"

namespace interface {

/**
 * @brief List of entries to choose one of them. Selection is moved with navigation keys or mouse
 * wheel (click selects the entry under mouse cursor), then it is chosen with Return, the same key
 * used to open picker or double-click, or cancelled with Escape/Close. What to do on each of these
 * actions is up to derived class, and where to place picker on screen is up to who renders it
 */
class Picker : public Element {
 public:
  /**
   * @brief Entry to choose from
   */
  struct Entry {
    std::string name;         //!< Text to identify entry
    std::string description;  //!< Text to describe entry (optional, shown beside its name)
  };

  /**
   * @brief Construct a new Picker object
   * @param title Text shown on top of picker
   * @param key Key used to open picker (which also chooses the selected entry while it is open)
   */
  Picker(const std::string& title, const keybinding::Key& key);

  /**
   * @brief Destroy Picker object
   */
  ~Picker() override = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled (always, when picker is open and it is modal), otherwise
   * false
   */
  bool OnEvent(const ftxui::Event& event) override;

  /**
   * @brief Indicates if picker is visible
   * @return true if picker is visible, otherwise false
   */
  bool IsVisible() const { return visible_; }

  /* ******************************************************************************************** */
  //! Operations for derived class
 protected:
  //! Replace entries to choose from
  void SetEntries(std::vector<Entry> entries) { entries_ = std::move(entries); }

  //! Get entries to choose from
  const std::vector<Entry>& GetEntries() const { return entries_; }

  //! Change selected entry, without notifying about it
  void SetSelected(size_t index) { selected_ = index; }

  //! Get index of selected entry
  size_t GetSelected() const { return selected_; }

  //! Set picker as visible
  void Show() { visible_ = true; }

  /* ******************************************************************************************** */
  //! Actions to implement by derived class

  //! Selected entry has changed while picker is open
  virtual void OnSelect(size_t /*index*/) { /* optional */ }

  //! Selected entry was chosen (picker is already closed)
  virtual void OnChoose(size_t index) = 0;

  //! Picker was closed without choosing any entry
  virtual void OnCancel() { /* optional */ }

  //! Modal picker is shown over all blocks, so it does not let them handle anything while it is
  //! open (otherwise, events not used by picker are still handled by others)
  virtual bool IsModal() const { return true; }

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  //! Handle event while picker is open, returning true only when it is used by picker
  bool HandleEvent(const ftxui::Event& event);

  //! Change selected entry, notifying about it
  void Select(size_t index);

  //! Select next/previous entry (if any)
  void Move(bool next);

  //! Close picker, choosing selected entry
  void Choose();

  //! Close picker, without choosing anything
  void Cancel();

  //! Get index of entry rendered at the position of mouse cursor (if any)
  std::optional<size_t> GetEntryAt(const ftxui::Mouse& mouse) const;

  /* ******************************************************************************************** */
  //! Mouse handling (called by Element, only when mouse cursor is over picker)

  //! Move selection
  void HandleWheel(const ftxui::Mouse::Button& button) override;

  //! Select entry under mouse cursor
  void HandleClick(ftxui::Event& event) override;

  //! Select entry under mouse cursor and choose it
  void HandleDoubleClick(ftxui::Event& event) override;

  /* ******************************************************************************************** */
  //! Variables

  std::string title_;           //!< Text shown on top of picker
  const keybinding::Key& key_;  //!< Key used to open picker

  std::vector<Entry> entries_;     //!< Entries to choose from
  std::vector<ftxui::Box> boxes_;  //!< Single box for each entry rendered (to handle mouse)

  size_t selected_ = 0;   //!< Index of selected entry
  bool visible_ = false;  //!< Picker is open
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_PICKER_H_
