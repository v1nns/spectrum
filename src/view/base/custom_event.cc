#include "view/base/custom_event.h"

namespace interface {

// Static
CustomEvent CustomEvent::UpdateFileInfo(const model::Song& info) {
  CustomEvent event;
  event.identifier_ = kUpdateFileInfo;
  event.content_ = info;

  return event;
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::ClearFileInfo() {
  CustomEvent event;
  event.identifier_ = kClearFileInfo;

  return event;
}

/* ********************************************************************************************** */

model::Song CustomEvent::GetContent() const { return std::get<model::Song>(content_); }

}  // namespace interface