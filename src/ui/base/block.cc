#include "ui/base/block.h"

#include "ui/base/terminal.h"

namespace interface {

Block::Block(std::shared_ptr<Dispatcher> const d, const unsigned int i) : dispatcher_(d), id_(i) {}

void Block::Send(BlockEvent event) { dispatcher_->Broadcast(shared_from_this(), event); }

}  // namespace interface