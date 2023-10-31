/**
 * \file
 * \brief  Class for a single tab view
 */

#ifndef INCLUDE_VIEW_ELEMENT_TAB_VIEW_H_
#define INCLUDE_VIEW_ELEMENT_TAB_VIEW_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "model/block_identifier.h"
#include "view/base/custom_event.h"
#include "view/base/event_dispatcher.h"
#include "view/base/keybinding.h"
#include "view/element/button.h"

namespace interface {

/**
 * @brief Element representing a single tab and its respective content
 */
class TabItem {
 protected:
  //! Callback to ask for focus on parent block
  using FocusCallback = std::function<void()>;

  //! Callback for button click event
  using ButtonCallback = Button::Callback;

  /**
   * @brief Construct a new TabItem object (only called by derived classes)
   * @param id Parent block identifier
   * @param dispatcher Event dispatcher
   * @param on_focus Callback function to ask for focus
   * @param keybinding Keybinding to set item as active
   * @param title Tab item title
   */
  explicit TabItem(const model::BlockIdentifier& id,
                   const std::shared_ptr<EventDispatcher>& dispatcher,
                   const FocusCallback& on_focus, const keybinding::Key& keybinding,
                   const std::string& title);

 public:
  /**
   * @brief Destroy the TabItem object
   */
  virtual ~TabItem() = default;

  /* ******************************************************************************************** */
  //! Must be implemented by derived class

  virtual ftxui::Element Render() = 0;

  /* ******************************************************************************************** */
  //! Implementation by derived is optional

  virtual bool OnEvent(const ftxui::Event&);
  virtual bool OnMouseEvent(ftxui::Event&);
  virtual bool OnCustomEvent(const CustomEvent&);

  /* ******************************************************************************************** */
  //! Getters

  //! Get mapped keybinding
  const keybinding::Key& GetKeybinding() const { return key_; }

  //! Get border button
  const WindowButton& GetButton() const { return button_; }

  /* ******************************************************************************************** */
  //! Variables
 protected:
  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  model::BlockIdentifier parent_id_;           //!< Parent block identifier

  FocusCallback on_focus_;  //!< Callback function to ask for focus on parent block

  keybinding::Key key_;  //!< Keybinding to set item as active
  std::string title_;    //!< Item title (to show on window border)
  WindowButton button_;  //!< Button to render in the tab border

  //!< Style for a button displayed as tab button
  static Button::ButtonStyle kTabButtonStyle;
};

/* ********************************************************************************************** */

/**
 * @brief Element for holding a group of tabs and render only the active one
 */
class Tab {
  // Tab identifier
  using View = int;
  static constexpr View kEmpty = -1;  //!< Invalid view identifier to represent empty view

  // Utility for TabItem
  using Content = std::unique_ptr<TabItem>;

 public:
  /**
   * @brief Construct a new TabItem object
   */
  Tab() = default;

  /**
   * @brief Destroy the TabItem object
   */
  ~Tab() = default;

  /* ******************************************************************************************** */
  //! Getters, setters and utilities

  //! Get active tab identifier
  View active() const { return active_; }

  //! Get active tab
  Content& active_item() { return views_[active_]; }

  //! Get all tabs
  std::map<View, Content>& items() { return views_; }

  //! Overloaded operator
  Content& operator[](const View& key) { return views_[key]; }

  //! Change active tab focus
  void SetActive(const View& item);

  /* ******************************************************************************************** */
  //! Variables
 private:
  View active_ = kEmpty;           //!< Current view displayed on tab
  std::map<View, Content> views_;  //!< All possible views to render in this component
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_TAB_VIEW_H_
