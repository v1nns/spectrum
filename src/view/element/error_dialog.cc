#include "view/element/error_dialog.h"

#include "view/base/keybinding.h"

namespace interface {

ErrorDialog::ErrorDialog()
    : Dialog(Size{.min_column = kMaxColumns, .min_line = kMaxLines},
             Style{.background = ftxui::Color::DarkRedBis, .foreground = ftxui::Color::Grey93}) {}

/* ********************************************************************************************** */

void ErrorDialog::SetErrorMessage(const std::string_view& message) {
  message_ = message;
  Open();
}

/* ********************************************************************************************** */

ftxui::Element ErrorDialog::RenderImpl(const ftxui::Dimensions& curr_size) const {
  return ftxui::vbox({
      ftxui::text(" ERROR") | ftxui::bold,
      ftxui::text(""),
      ftxui::paragraph(message_) | ftxui::center | ftxui::bold,
  });
}

/* ********************************************************************************************** */

bool ErrorDialog::OnEventImpl(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;
  if (event == Keybind::Return) {
    Close();
  }

  return true;
}

/* ********************************************************************************************** */

bool ErrorDialog::OnMouseEventImpl(ftxui::Event event) { return false; }

}  // namespace interface
