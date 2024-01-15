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
using FileMenu = std::unique_ptr<internal::Menu<internal::FileMenu>>;

/* --------------------------------------- Menu creation ---------------------------------------- */

namespace menu {

/**
 * @brief Construct a new FileMenu object
 * @param dispatcher Event dispatcher
 * @param force_refresh Callback function to update UI
 * @param on_click Callback function for click event on active entry
 */
FileMenu CreateFileMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                        const TextAnimation::Callback& force_refresh,
                        const internal::FileMenu::Callback& on_click);

}  // namespace menu
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_MENU_H_
