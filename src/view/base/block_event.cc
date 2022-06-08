#include "view/base/block_event.h"

namespace interface {

BlockEvent BlockEvent::Special(std::string name) {
  BlockEvent e;
  e.type_ = std::move(name);
  return e;
}

BlockEvent BlockEvent::UpdateFileInfo = BlockEvent::Special("UpdateFileInfo");

}  // namespace interface
