#include "view/base/custom_event.h"

namespace interface {

// Static
CustomEvent CustomEvent::UpdateFileInfo(const model::Song& info) {
  CustomEvent event;
  event.type_ = Type::UpdateFileInfo;
  event.content_ = info;

  return event;
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ClearFileInfo() {
  CustomEvent event;
  event.type_ = Type::ClearFileInfo;

  return event;
}

}  // namespace interface