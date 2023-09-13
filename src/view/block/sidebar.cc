#include "view/block/sidebar.h"

#include <memory>

#include "util/logger.h"
#include "view/block/sidebar_content/list_directory.h"

namespace interface {

Sidebar::Sidebar(const std::shared_ptr<EventDispatcher>& dispatcher,
                 const std::string& optional_path)
    : Block{dispatcher, model::BlockIdentifier::Sidebar,
            interface::Size{.width = kMaxColumns, .height = 0}},
      tab_elem_{} {
  // Create all tabs
  tab_elem_[View::Files] =
      std::make_unique<ListDirectory>(GetId(), dispatcher, std::bind(&Sidebar::AskForFocus, this),
                                      keybinding::Sidebar::FocusList, kMaxColumns, optional_path);

  // Set visualizer as active tab
  tab_elem_.SetActive(View::Files);
}

/* ********************************************************************************************** */

ftxui::Element Sidebar::Render() {
  ftxui::Elements buttons;

  // Append tab buttons
  for (const auto& [id, item] : tab_elem_.items()) {
    buttons.emplace_back(item->GetButton()->Render());
  }

  ftxui::Element title_border = ftxui::hbox(buttons);

  ftxui::Element view = tab_elem_.active_item()->Render();

  return ftxui::window(title_border, view | ftxui::yflex) | GetBorderDecorator();
}

/* ********************************************************************************************** */

bool Sidebar::OnEvent(ftxui::Event event) {
  if (event.is_mouse()) return OnMouseEvent(event);

  // Check if event is equal to a registered keybinding for any of the tab items
  if (auto found =
          std::find_if(tab_elem_.items().begin(), tab_elem_.items().end(),
                       [&event](const auto& t) { return t.second->GetKeybinding() == event; });
      found != tab_elem_.items().end()) {
    // Ask for focus if block is not focused
    if (!IsFocused()) {
      LOG("Asking for focus on Sidebar block");
      AskForFocus();
    }

    // Change tab item selected
    if (tab_elem_.active() != found->first) {
      // TODO: instead of index, print view name
      LOG("Handle key to change tab item selected from ", tab_elem_.active(), " to ", found->first);
      tab_elem_.SetActive(found->first);
    }

    return true;
  }

  // If block is not focused, do not even try to handle event
  if (!IsFocused()) {
    return false;
  }

  // Otherwise, let item handle it
  return tab_elem_.active_item()->OnEvent(event);
}

/* ********************************************************************************************** */

bool Sidebar::OnCustomEvent(const CustomEvent& event) {
  return tab_elem_.active_item()->OnCustomEvent(event);
}

/* ********************************************************************************************** */

void Sidebar::OnFocus() {
  // Update internal state for all buttons
  for (const auto& [id, item] : tab_elem_.items()) item->GetButton()->UpdateParentFocus(true);
}

/* ********************************************************************************************** */

void Sidebar::OnLostFocus() {
  // Update internal state for all buttons
  for (const auto& [id, item] : tab_elem_.items()) item->GetButton()->UpdateParentFocus(false);
}

/* ********************************************************************************************** */

bool Sidebar::OnMouseEvent(ftxui::Event event) {
  for (const auto& [id, item] : tab_elem_.items()) {
    if (item->GetButton()->OnMouseEvent(event)) {
      tab_elem_.SetActive(id);
      return true;
    }
  }

  return tab_elem_.active_item()->OnMouseEvent(event);
}

}  // namespace interface
