#include "view/element/tab_item.h"

#include "util/logger.h"

namespace interface {

TabItem::TabItem(const model::BlockIdentifier& id,
                 const std::shared_ptr<EventDispatcher>& dispatcher)
    : dispatcher_{dispatcher}, parent_id_{id} {}

/* ********************************************************************************************** */

bool TabItem::OnEvent(const ftxui::Event&) { return false; }

/* ********************************************************************************************** */

bool TabItem::OnCustomEvent(const CustomEvent&) { return false; }

/* ********************************************************************************************** */

bool TabItem::OnMouseEvent(const ftxui::Event&) { return false; }

}  // namespace interface