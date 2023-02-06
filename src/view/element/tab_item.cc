#include "view/element/tab_item.h"

#include "util/logger.h"

namespace interface {

TabItem::TabItem(const std::shared_ptr<EventDispatcher>& dispatcher) : dispatcher_{dispatcher} {}

/* ********************************************************************************************** */

bool TabItem::OnEvent(ftxui::Event) { return false; }

/* ********************************************************************************************** */

bool TabItem::OnCustomEvent(const CustomEvent&) { return false; }

/* ********************************************************************************************** */

bool TabItem::OnMouseEvent(ftxui::Event event) { return false; }

}  // namespace interface