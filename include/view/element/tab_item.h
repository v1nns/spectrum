/**
 * \file
 * \brief  Class for a single tab view
 */

#ifndef INCLUDE_VIEW_ELEMENT_TAB_VIEW_H_
#define INCLUDE_VIEW_ELEMENT_TAB_VIEW_H_

#include <memory>
#include <string>

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "model/block_identifier.h"
#include "view/base/custom_event.h"
#include "view/base/event_dispatcher.h"

namespace interface {

/**
 * @brief Component to display a single tab and its respective content
 */
class TabItem {
 public:
  //! Callback to ask for focus on parent block
  using FocusCallback = std::function<void()>;

 protected:
  /**
   * @brief Construct a new TabItem object (only called by derived classes)
   * @param id Parent block identifier
   * @param dispatcher Event dispatcher
   * @param on_focus Callback function to ask for focus
   */
  explicit TabItem(const model::BlockIdentifier& id,
                   const std::shared_ptr<EventDispatcher>& dispatcher,
                   const FocusCallback& on_focus);

 public:
  /**
   * @brief Destroy the TabItem object
   */
  virtual ~TabItem() = default;

  /* ******************************************************************************************** */
  //! Remove these
  TabItem(const TabItem& other) = delete;             // copy constructor
  TabItem(TabItem&& other) = delete;                  // move constructor
  TabItem& operator=(const TabItem& other) = delete;  // copy assignment
  TabItem& operator=(TabItem&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
  //! Must be implemented by derived class

  virtual ftxui::Element Render() = 0;

  /* ******************************************************************************************** */
  //! Implementation by derived is optional

  virtual bool OnEvent(const ftxui::Event&);
  virtual bool OnMouseEvent(const ftxui::Event&);
  virtual bool OnCustomEvent(const CustomEvent&);

  /* ******************************************************************************************** */
  //! Variables
 protected:
  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  model::BlockIdentifier parent_id_;           //!< Parent block identifier
  FocusCallback on_focus_;  //!< Callback function to ask for focus on parent block
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_TAB_VIEW_H_
