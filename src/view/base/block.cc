#include "view/base/block.h"

#include "view/base/terminal.h"

namespace interface {

Block::Block(const std::shared_ptr<EventDispatcher>& d, const unsigned int id)
    : ComponentBase(), dispatcher_(d), listener_(nullptr), id_(id) {}

/* ********************************************************************************************** */

void Block::SetActionListener(const std::shared_ptr<ActionListener>& listener) {
  listener_ = listener;
}

/* ********************************************************************************************** */

void Block::Send(BlockEvent event) { dispatcher_->Broadcast(this, event); }

}  // namespace interface