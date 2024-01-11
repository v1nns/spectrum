/**
 * \file
 * \brief  Header containing all menu specializations
 */

#ifndef INCLUDE_VIEW_ELEMENT_MENU_H_
#define INCLUDE_VIEW_ELEMENT_MENU_H_

#include "view/element/internal/file_menu.h"
#include "view/element/internal/menu.h"

namespace interface {

//! Aliases for all existing menu specializations
using FileMenu = std::unique_ptr<Menu<internal::FileMenu>>;

/* --------------------------------------- Menu creation ---------------------------------------- */

namespace menu {

/**
 * @brief Construct a new FileMenu object
 * @param force_refresh Callback function to update UI
 * @param on_click Callback function for click event on active entry
 */
inline FileMenu CreateFileMenu(const TextAnimation::Callback& force_refresh,
                               const internal::FileMenu::Callback& on_click) {
  return std::make_unique<internal::FileMenu>(force_refresh, on_click);
}

}  // namespace menu
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_MENU_H_
