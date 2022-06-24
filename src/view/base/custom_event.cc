#include "view/base/custom_event.h"

namespace interface {

// Static
CustomEvent CustomEvent::ClearSongInfo() {
  CustomEvent event;
  event.type_ = Type::ClearSongInfo;

  return event;
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::UpdateSongInfo(const model::Song& info) {
  CustomEvent event;
  event.type_ = Type::UpdateSongInfo;
  event.content_ = info;

  return event;
}
/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::UpdateSongState(const model::Song::State& new_state) {
  CustomEvent event;
  event.type_ = Type::UpdateSongState;
  event.content_ = new_state;

  return event;
}

/* ********************************************************************************************** */

// Static
CustomEvent CustomEvent::NotifyFileSelection(const std::filesystem::path file_path) {
  CustomEvent event;
  event.type_ = Type::NotifyFileSelection;
  event.content_ = file_path;

  return event;
}

}  // namespace interface