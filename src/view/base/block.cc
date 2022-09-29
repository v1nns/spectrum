#include "view/base/block.h"

#include "view/base/event_dispatcher.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& dispatcher, const Identifier id,
             const Size& size)
    : ftxui::ComponentBase{}, id_{id}, dispatcher_{dispatcher}, size_{size} {}

}  // namespace interface
