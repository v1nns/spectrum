/**
 * \file
 * \brief  Header containing all menu specializations
 */

#ifndef INCLUDE_VIEW_ELEMENT_MENU_H_
#define INCLUDE_VIEW_ELEMENT_MENU_H_

#include "view/element/internal/base_menu.h"
#include "view/element/internal/file_menu.h"
#include "view/element/internal/playlist_menu.h"
#include "view/element/internal/song_menu.h"

namespace interface {

//! Aliases for all existing menu specializations
using FileMenu = std::unique_ptr<internal::BaseMenu<internal::FileMenu>>;
using PlaylistMenu = std::unique_ptr<internal::BaseMenu<internal::PlaylistMenu>>;
using SongMenu = std::unique_ptr<internal::BaseMenu<internal::SongMenu>>;

/* --------------------------------------- Menu creation ---------------------------------------- */

namespace menu {

/**
 * @brief Construct a new FileMenu object
 * @param dispatcher Event dispatcher
 * @param file_handler Utility handler to manage any file operation
 * @param force_refresh Callback function to update UI
 * @param on_click Callback function for click event on active entry
 * @param style Theme selection
 * @param optional_path Path to list files from
 */
FileMenu CreateFileMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                        const std::shared_ptr<util::FileHandler>& file_handler,
                        const TextAnimation::Callback& force_refresh,
                        const internal::FileMenu::Callback& on_click, const Style& style,
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
 * @brief Construct a new SongMenu object
 * @param dispatcher Event dispatcher
 * @param force_refresh Callback function to update UI
 * @param on_click Callback function for click event on active entry
 */
SongMenu CreateSongMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                        const TextAnimation::Callback& force_refresh,
                        const internal::SongMenu::Callback& on_click);

}  // namespace menu
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_MENU_H_
