#include "view/base/block.h"

#include "view/base/event_dispatcher.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& dispatcher, const Identifier id)
    : ftxui::ComponentBase{}, id_{id}, dispatcher_{dispatcher} {}

}  // namespace interface