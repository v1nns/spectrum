#include "view/base/element.h"

namespace interface {

ftxui::Element Element::Render() { return ftxui::text(""); }

/* ********************************************************************************************** */

bool Element::OnEvent(const ftxui::Event& event) {
  // Optional implementation
  return false;
}

/* ********************************************************************************************** */

bool Element::OnMouseEvent(ftxui::Event& event) {
  if (event.mouse().button != ftxui::Mouse::Left && event.mouse().button != ftxui::Mouse::None)
    return false;

  if (!box_.Contain(event.mouse().x, event.mouse().y)) {
    hovered_ = false;
    return false;
  }

  hovered_ = true;

  if (event.mouse().button == ftxui::Mouse::WheelDown ||
      event.mouse().button == ftxui::Mouse::WheelUp) {
    HandleWheel(event.mouse().button);
    return true;
  } else if (event.mouse().button == ftxui::Mouse::Left &&
             event.mouse().motion == ftxui::Mouse::Released) {
    // Check if this is a double-click event
    auto now = std::chrono::system_clock::now();

    if (now - last_click_ <= std::chrono::milliseconds(500))
      HandleDoubleClick(event);
    else
      HandleClick(event);

    last_click_ = now;
    return true;
  } else {
    HandleHover(event);
  }

  return false;
}

/* ********************************************************************************************** */

bool Element::HandleActionKey(const ftxui::Event& event) {
  // Optional implementation
  return false;
}

/* ********************************************************************************************** */

void Element::HandleClick(ftxui::Event& event) {
  // Optional implementation
}

/* ********************************************************************************************** */

void Element::HandleDoubleClick(ftxui::Event& event) {
  // Optional implementation
}

/* ********************************************************************************************** */

void Element::HandleHover(ftxui::Event& event) {
  // Optional implementation
}

/* ********************************************************************************************** */

void Element::HandleWheel(const ftxui::Mouse::Button& button) {
  // Optional implementation
}

}  // namespace interface
