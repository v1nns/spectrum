#include "view/element/question_dialog.h"

#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {

QuestionDialog::QuestionDialog()
    : Dialog(Size{.min_column = kMaxColumns, .min_line = kMaxLines},
             Style{.background = ftxui::Color::SteelBlue, .foreground = ftxui::Color::Grey93}) {
  auto style = Button::Style{
      .normal =
          Button::Style::State{
              .foreground = ftxui::Color::Black,
              .background = ftxui::Color::SteelBlue1,
          },
      .focused =
          Button::Style::State{
              .foreground = ftxui::Color::DeepSkyBlue4Ter,
              .background = ftxui::Color::LightSkyBlue1,
          },
      .pressed =
          Button::Style::State{
              .foreground = ftxui::Color::SkyBlue1,
              .background = ftxui::Color::Blue1,
          },
      .width = 15,
  };

  using ftxui::text, ftxui::hbox;

  // Default style to highlight letter used as keybinding
  ftxui::Decorator highlight =
      ftxui::color(ftxui::Color::DeepPink4Bis) | ftxui::underlined | ftxui::bold;

  btn_yes_ = Button::make_button_custom(
      hbox({text("Y") | highlight, text("es")}),
      [this]() {
        LOG("Handle \"yes\" button");
        if (content_->cb_yes) content_->cb_yes();

        Close();
        return true;
      },
      style);

  btn_no_ = Button::make_button_custom(
      hbox({text("N") | highlight, text("o")}),
      [this]() {
        LOG("Handle \"no\" button");
        if (content_->cb_no) content_->cb_no();

        Close();
        return true;
      },
      style);
}

/* ********************************************************************************************** */

void QuestionDialog::SetMessage(const model::QuestionData& data) { content_ = data; }

/* ********************************************************************************************** */

ftxui::Element QuestionDialog::RenderImpl(const ftxui::Dimensions& curr_size) const {
  return ftxui::vbox({
      ftxui::text(""),
      ftxui::paragraph(content_->question) | ftxui::center | ftxui::bold |
          ftxui::color(ftxui::Color::Black),
      ftxui::text(""),
      ftxui::hbox(btn_yes_->Render(), ftxui::text("  "), btn_no_->Render()) | ftxui::flex |
          ftxui::center | ftxui::bold,
  });
}

/* ********************************************************************************************** */

bool QuestionDialog::OnEventImpl(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (event == keybinding::Dialog::Yes) {
    btn_yes_->OnClick();
    return true;
  }

  if (event == keybinding::Dialog::No) {
    btn_no_->OnClick();
    return true;
  }

  if (event == Keybind::Escape || event == Keybind::Close) {
    Close();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool QuestionDialog::OnMouseEventImpl(ftxui::Event event) {
  if (btn_yes_->OnMouseEvent(event)) return true;
  if (btn_no_->OnMouseEvent(event)) return true;

  return false;
}

}  // namespace interface
