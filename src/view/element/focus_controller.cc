#include "view/element/focus_controller.h"

#include "util/formatter.h"
#include "util/logger.h"

namespace interface {

bool FocusController::OnEvent(const ftxui::Event& event) {
  // Navigate on elements
  if (event == Keybinding::ArrowRight || event == Keybinding::Right) {
    LOG("Handle navigation key=", util::EventToString(event));

    // Calculate new index based on upper bound
    int new_index =
        focus_index_ + (focus_index_ < (static_cast<int>(elements_.size()) - 1) ? 1 : 0);
    UpdateFocus(focus_index_, new_index);

    return true;
  }

  // Navigate on elements
  if (event == Keybinding::ArrowLeft || event == Keybinding::Left) {
    LOG("Handle navigation key=", util::EventToString(event));

    // Calculate new index based on lower bound
    int new_index = focus_index_ - (focus_index_ > (kInvalidIndex + 1) ? 1 : 0);
    UpdateFocus(focus_index_, new_index);

    return true;
  }

  if (HasElementFocused()) {
    // Pass event to element if mapped as navigation key
    if (auto found = std::find(action_events.begin(), action_events.end(), event);
        found != action_events.end() && elements_[focus_index_]->HandleActionKey(event)) {
      LOG("Element with index=", focus_index_, "handled action key=", util::EventToString(event));
      return true;
    }

    if (elements_[focus_index_]->OnEvent(event)) {
      LOG("Element with index=", focus_index_, "handled event=", util::EventToString(event));
      return true;
    }

    // Remove focus state from element
    if (event == Keybinding::Escape) {
      // Invalidate old index for focused
      LOG("Handle navigation key=", util::EventToString(event));
      UpdateFocus(focus_index_, kInvalidIndex);

      return true;
    }
  }

  return false;
}

/* ********************************************************************************************** */

bool FocusController::OnMouseEvent(ftxui::Event& event) {
  // Iterate through all elements and pass event, if event is handled, update UI state
  bool event_handled = std::any_of(elements_.begin(), elements_.end(), [&event](Element* element) {
    if (!element) return false;
    return element->OnMouseEvent(event);
  });

  return event_handled;
}

/* ********************************************************************************************** */

void FocusController::SetFocus(int index) {
  if (!elements_.empty() && (index + 1) <= elements_.size()) {
    UpdateFocus(focus_index_, index);
  }
}

/* ********************************************************************************************** */

void FocusController::UpdateFocus(int old_index, int new_index) {
  // If equal, do nothing
  if (old_index == new_index) return;

  // Remove focus from old focused frequency bar
  if (old_index != kInvalidIndex) elements_[old_index]->SetFocus(false);

  // Set focus on newly-focused frequency bar
  if (new_index != kInvalidIndex) elements_[new_index]->SetFocus(true);

  // Update internal index
  focus_index_ = new_index;
}

}  // namespace interface
