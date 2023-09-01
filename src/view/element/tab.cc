#include "view/element/tab.h"

#include "util/logger.h"

namespace interface {

/* ****************************************** TabItem ******************************************* */

static const Button::ButtonStyle kTabButtonStyle = Button::ButtonStyle{
    .normal =
        Button::ButtonStyle::State{
            .foreground = ftxui::Color::GrayDark,
            .background = ftxui::Color(),
        },
    .focused =
        Button::ButtonStyle::State{
            .foreground = ftxui::Color::GrayLight,
            .background = ftxui::Color::GrayDark,
        },
    .selected =
        Button::ButtonStyle::State{
            .foreground = ftxui::Color::DarkBlue,
            .background = ftxui::Color::DodgerBlue1,
        },

    .delimiters = Button::Delimiters{" ", " "},
};

/* ********************************************************************************************** */

TabItem::TabItem(const model::BlockIdentifier& id,
                 const std::shared_ptr<EventDispatcher>& dispatcher, const FocusCallback& on_focus,
                 const std::string& keybinding, const std::string& title)
    : dispatcher_{dispatcher},
      parent_id_{id},
      on_focus_{on_focus},
      key_{keybinding},
      title_{title},
      button_{Button::make_button_for_window(
          keybinding + ":" + title,
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

bool TabItem::OnCustomEvent(const CustomEvent&) { return false; }

/* ********************************************************************************************** */

bool TabItem::OnMouseEvent(const ftxui::Event&) { return false; }

/* ******************************************** Tab ********************************************* */

void Tab::SetActive(const View& item) {
  if (active_ != kEmpty) {
    // Unselect window button from old active item
    active_item()->GetButton()->Unselect();
  }

  // Update active tab identifier and button state
  active_ = item;
  active_item()->GetButton()->Select();
}

}  // namespace interface
