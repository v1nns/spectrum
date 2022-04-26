#include "view/base/block.h"

#include "view/base/terminal.h"

namespace interface {

Block::Block(const std::shared_ptr<Dispatcher>& d, const unsigned int id)
    : dispatcher_(d), id_(id) {}

/* ********************************************************************************************** */

void Block::Send(BlockEvent event) { dispatcher_->Broadcast(this, event); }

}  // namespace interface