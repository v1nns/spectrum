/**
 * \file
 * \brief  Header containing all menu specializations
 */

#ifndef INCLUDE_VIEW_ELEMENT_MENU_H_
#define INCLUDE_VIEW_ELEMENT_MENU_H_

#include "view/element/internal/base_menu.h"
#include "view/element/internal/file_menu.h"
#include "view/element/internal/playlist_menu.h"
#include "view/element/internal/text_menu.h"

namespace interface {

//! Aliases for all existing menu specializations
using FileMenu = std::unique_ptr<internal::BaseMenu<internal::FileMenu>>;
using PlaylistMenu = std::unique_ptr<internal::BaseMenu<internal::PlaylistMenu>>;
using TextMenu = std::unique_ptr<internal::BaseMenu<internal::TextMenu>>;

/* --------------------------------------- Menu creation ---------------------------------------- */

namespace menu {

/**
 * @brief Construct a new FileMenu object
 * @param dispatcher Event dispatcher
 * @param file_handler Utility handler to manage any file operation
 * @param force_refresh Callback function to update UI
 * @param on_click Callback function for click event on active entry
 */
FileMenu CreateFileMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                        const std::shared_ptr<util::FileHandler>& file_handler,
                        const TextAnimation::Callback& force_refresh,
                        const internal::FileMenu::Callback& on_click,
                        const std::string& optional_path = "");

/**
 * @brief Construct a new PlaylistMenu object
 * @param dispatcher Event dispatcher
 * @param force_refresh Callback function to update UI
 * @param on_click Callback function for click event on active entry
 */
PlaylistMenu CreatePlaylistMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                                const TextAnimation::Callback& force_refresh,
                                const internal::PlaylistMenu::Callback& on_click);

/**
 * @brief Construct a new TextMenu object
 * @param dispatcher Event dispatcher
 * @param force_refresh Callback function to update UI
 * @param on_click Callback function for click event on active entry
 */
TextMenu CreateTextMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                        const TextAnimation::Callback& force_refresh,
                        const internal::TextMenu::Callback& on_click);

}  // namespace menu
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_MENU_H_
