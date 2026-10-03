#include "view/element/error_dialog.h"

#include <string>
#include <string_view>

#include "ftxui/dom/elements.hpp"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

ErrorDialog::ErrorDialog(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Dialog(dispatcher, Size{.min_column = kMaxColumns, .min_line = kMaxLines},
             Style{.background = &Theme::Dialog::background_error,
                   .foreground = &Theme::Dialog::foreground}) {}

/* ********************************************************************************************** */

void ErrorDialog::SetErrorMessage(const std::string_view& message, const std::string& detail) {
  message_ = message;
  detail_ = detail;

  // Make room for detail only when there is one
  SetMinimumLines(detail_.empty() ? kMaxLines : kMaxLines + kDetailLines);

  Open();
}

/* ********************************************************************************************** */

ftxui::Element ErrorDialog::RenderImpl(const ftxui::Dimensions& curr_size) const {
  ftxui::Elements content{
      ftxui::text(" ERROR") | ftxui::bold,
      ftxui::text(""),
      ftxui::paragraph(message_) | ftxui::center | ftxui::bold,
  };

  if (!detail_.empty()) {
    content.push_back(ftxui::text(""));
    content.push_back(ftxui::paragraph(detail_) | ftxui::center);
  }

  return ftxui::vbox(content);
}

/* ********************************************************************************************** */

bool ErrorDialog::OnEventImpl(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (event == Keybind::Return) {
    Close();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool ErrorDialog::OnMouseEventImpl(ftxui::Event event) { return false; }

/* ********************************************************************************************** */

void ErrorDialog::OnClose() {
  message_.clear();
  detail_.clear();

  auto dispatcher = GetDispatcher();
  if (!dispatcher) return;

  auto event_closed = interface::CustomEvent::NotifyDialogClosed();
  dispatcher->SendEvent(event_closed);
}

}  // namespace interface
