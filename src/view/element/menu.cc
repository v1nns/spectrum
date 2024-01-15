#include "view/element/menu.h"

namespace interface {
namespace menu {

FileMenu CreateFileMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                        const TextAnimation::Callback& force_refresh,
                        const internal::FileMenu::Callback& on_click) {
  return std::make_unique<internal::FileMenu>(dispatcher, force_refresh, on_click);
}

}  // namespace menu
}  // namespace interface
