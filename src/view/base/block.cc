#include "view/base/block.h"

#include "view/base/event_dispatcher.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& dispatcher, const BlockIdentifier id)
    : ftxui::ComponentBase{}, id_{id}, dispatcher_{dispatcher}, listener_{} {}

/* ********************************************************************************************** */

void Block::Attach(const std::shared_ptr<ActionListener>& listener) { listener_ = listener; }

}  // namespace interface