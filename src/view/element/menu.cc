#include "view/element/menu.h"

namespace interface {
namespace menu {

FileMenu CreateFileMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                        const std::shared_ptr<util::FileHandler>& file_handler,
                        const TextAnimation::Callback& force_refresh,
                        const internal::FileMenu::Callback& on_click,
                        const std::string& optional_path) {
  return std::make_unique<internal::FileMenu>(dispatcher, file_handler, force_refresh, on_click,
                                              optional_path);
}

/* ********************************************************************************************** */

PlaylistMenu CreatePlaylistMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                                const TextAnimation::Callback& force_refresh,
                                const internal::PlaylistMenu::Callback& on_click) {
  return std::make_unique<internal::PlaylistMenu>(dispatcher, force_refresh, on_click);
}

}  // namespace menu
}  // namespace interface
