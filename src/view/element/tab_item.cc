#include "view/element/tab_item.h"

namespace interface {

TabItem::TabItem(const std::shared_ptr<EventDispatcher>& dispatcher, const std::string& title)
    : dispatcher_{dispatcher}, title_{title} {}

}  // namespace interface