#include "view/element/question_dialog.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/string.hpp"
#include "util/logger.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

namespace {

//! Lines used by everything else in dialog: an empty line before and after question, and buttons
constexpr int kOtherLines = 3;

//! Get number of lines used by text when it is wrapped (at spaces) to fit in the given columns
int CountWrappedLines(const std::string& text, int columns) {
  std::istringstream words{text};

  int lines = 1;
  int used = 0;

  for (std::string word; words >> word;) {
    const int width = ftxui::string_width(word);

    if (used > 0 && used + 1 + width > columns) {
      lines++;
      used = 0;
    }

    used += (used > 0 ? 1 : 0) + width;
  }

  return lines;
}

}  // namespace

/* ********************************************************************************************** */

QuestionDialog::QuestionDialog(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Dialog(dispatcher, Size{.min_column = kMaxColumns, .min_line = kMaxLines},
             Style{.background = &Theme::Dialog::background,
                   .foreground = &Theme::Dialog::foreground}) {
  auto style = Button::Style{
      // Selected state is used by button activated by Return key
      .colors = [] { return GetTheme().dialog.answer; },
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

  // Question is wrapped when it does not fit in a single line (e.g. playlist with a long name),
  // so make room for all of its lines, otherwise buttons would not be shown
  SetMinimumLines(std::max(kMaxLines, CountWrappedLines(data.question, kMaxColumns) + kOtherLines));

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
          ftxui::color(GetTheme().dialog.text),
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
