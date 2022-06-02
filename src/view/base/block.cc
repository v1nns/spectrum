#include "view/base/block.h"

#include "view/base/event_dispatcher.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& d, const BlockIdentifier id)
    : ftxui::ComponentBase{}, id_{id}, dispatcher_{d}, listener_{} {}

/* ********************************************************************************************** */

void Block::Attach(const std::shared_ptr<ActionListener>& listener) { listener_ = listener; }

}  // namespace interface