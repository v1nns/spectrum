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
#include "view/base/custom_event.h"
#include "view/base/event_dispatcher.h"

namespace interface {

/**
 * @brief Component to display a single tab and its respective content
 */
class TabItem {
 protected:
  /**
   * @brief Construct a new TabItem object (only called by derived classes)
   * @param dispatcher Event dispatcher
   * @param title Title for tab item
   */
  explicit TabItem(const std::shared_ptr<EventDispatcher>& dispatcher, const std::string& title);

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

  //! Get title
  std::string GetTitle() const { return title_; }

  /* ******************************************************************************************** */
  //! These must be implemented by derived class

  virtual ftxui::Element Render() = 0;
  virtual bool OnEvent(ftxui::Event) = 0;
  virtual bool OnMouseEvent(ftxui::Event event) = 0;
  virtual bool OnCustomEvent(const CustomEvent&) = 0;

  /* ******************************************************************************************** */
  //! Variables
 protected:
  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  std::string title_;                          //!< Tab item title
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_TAB_VIEW_H_