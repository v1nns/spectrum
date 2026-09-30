#include "view/element/question_dialog.h"

#include "ftxui/dom/elements.hpp"
#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {

QuestionDialog::QuestionDialog(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Dialog(dispatcher, Size{.min_column = kMaxColumns, .min_line = kMaxLines},
             Style{.background = ftxui::Color::SteelBlue, .foreground = ftxui::Color::Grey93}) {
  auto style = Button::Style{
      .normal =
          Button::Style::State{
              .foreground = ftxui::Color::Black,
              .background = ftxui::Color::SteelBlue1,
          },
      .focused =
          Button::Style::State{
              .foreground = ftxui::Color::Black,
              .background = ftxui::Color::LightSkyBlue1,
          },
      // Button activated by Return key
      .selected =
          Button::Style::State{
              .foreground = ftxui::Color::Black,
              .background = ftxui::Color::Grey93,
          },
      .pressed =
          Button::Style::State{
              .foreground = ftxui::Color::SkyBlue1,
              .background = ftxui::Color::Blue1,
          },
      .highlight =
          Button::Style::State{
              .foreground = ftxui::Color::DarkRed,
          },

      .delimiters = Button::Delimiters(" ", " "),
  };

  btn_yes_ = Button::make_button(
      "Yes",
      [this]() {
        LOG("Handle \"yes\" button");
        if (content_->cb_yes) content_->cb_yes();

        Close();
        return true;
      },
      style, "Y");

  btn_no_ = Button::make_button(
      "No",
      [this]() {
        LOG("Handle \"no\" button");
        if (content_->cb_no) content_->cb_no();

        Close();
        return true;
      },
      style, "N");
}

/* ********************************************************************************************** */

void QuestionDialog::SetMessage(const model::QuestionData& data) {
  content_ = data;

  // Always start with the safest option
  SelectButton(false);
}

/* ********************************************************************************************** */

void QuestionDialog::SelectButton(bool yes) {
  yes_selected_ = yes;

  if (yes_selected_) {
    btn_yes_->Select();
    btn_no_->Unselect();
  } else {
    btn_no_->Select();
    btn_yes_->Unselect();
  }
}

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

  // Move selection between buttons
  if (event == Keybind::ArrowLeft || event == Keybind::ArrowRight || event == Keybind::Left ||
      event == Keybind::Right || event == Keybind::Tab || event == Keybind::TabReverse) {
    SelectButton(!yes_selected_);
    return true;
  }

  // Press selected button
  if (event == Keybind::Return) {
    LOG("Handle key to press selected button");
    (yes_selected_ ? btn_yes_ : btn_no_)->OnClick();
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
