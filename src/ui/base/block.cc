#include "ui/base/block.h"

#include "ui/base/terminal.h"

namespace interface {

BlockEvent BlockEvent::Special(std::string name) {
  BlockEvent e;
  e.type_ = std::move(name);
  return e;
}

BlockEvent BlockEvent::FileSelected = BlockEvent::Special("FileSelected");

/* ********************************************************************************************** */

Block::Block(const std::shared_ptr<Dispatcher>& d, const unsigned int id)
    : dispatcher_(d), id_(id) {}

/* ********************************************************************************************** */

void Block::Send(BlockEvent event) { dispatcher_->Broadcast(this, event); }

}  // namespace interface