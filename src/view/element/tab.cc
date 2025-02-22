#include "view/element/tab.h"

#include <iomanip>

#include "util/formatter.h"
#include "util/logger.h"

namespace interface {

/* ****************************************** TabItem ******************************************* */

//!  TabItem pretty print
std::ostream& operator<<(std::ostream& out, const TabItem& item) {
  out << std::quoted(item.title_);
  return out;
}

Button::Style TabItem::kTabButtonStyle = Button::Style{
    .normal =
        Button::Style::State{
            .foreground = ftxui::Color::GrayDark,
            .background = ftxui::Color(),
        },
    .focused =
        Button::Style::State{
            .foreground = ftxui::Color::GrayLight,
            .background = ftxui::Color::GrayDark,
        },
    .selected =
        Button::Style::State{
            .foreground = ftxui::Color::LightSteelBlue1,
            .background = ftxui::Color::SteelBlue3,
        },

    .delimiters = Button::Delimiters{" ", " "},
};

/* ********************************************************************************************** */

TabItem::TabItem(const model::BlockIdentifier& id,
                 const std::shared_ptr<EventDispatcher>& dispatcher, const FocusCallback& on_focus,
                 const keybinding::Key& keybinding, const std::string& title)
    : dispatcher_{dispatcher},
      parent_id_{id},
      on_focus_{on_focus},
      key_{keybinding},
      title_{title},
      button_{Button::make_button_for_window(
          util::EventToString(keybinding) + ":" + title,
          [this]() {
            LOG("Handle left click mouse event on Tab button for ", title_);

            // Send event to set focus on this block
            on_focus_();

            return true;
          },
          kTabButtonStyle)} {}

/* ********************************************************************************************** */

bool TabItem::OnEvent(const ftxui::Event&) { return false; }

/* ********************************************************************************************** */

bool TabItem::OnMouseEvent(ftxui::Event&) { return false; }

/* ********************************************************************************************** */

bool TabItem::OnCustomEvent(const CustomEvent&) { return false; }

/* ********************************************************************************************** */

void TabItem::OnFocus() {
  // do nothing
}

/* ********************************************************************************************** */

void TabItem::OnLostFocus() {
  // do nothing
}

/* ******************************************** Tab ********************************************* */

void Tab::SetActive(const View& item) {
  if (active_ != kEmpty) {
    // Unselect window button from old active item
    active_item()->GetButton()->Unselect();
    active_item()->OnLostFocus();
  }

  // Update active tab identifier and button state
  active_ = item;
  active_item()->GetButton()->Select();
  active_item()->OnFocus();
}

}  // namespace interface
