#include "view/base/block.h"

#include "view/base/terminal.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& d, const BlockIdentifier id)
    : ftxui::ComponentBase(), id_(id), dispatcher_(d), listener_(nullptr) {}

/* ********************************************************************************************** */

void Block::Attach(const std::shared_ptr<ActionListener>& listener) { listener_ = listener; }

/* ********************************************************************************************** */

void Block::Send(BlockEvent event) { dispatcher_->Broadcast(this, event); }

}  // namespace interface