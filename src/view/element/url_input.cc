#include "view/element/url_input.h"

#include <cctype>
#include <iomanip>

#include "util/formatter.h"
#include "util/logger.h"
#include "view/base/keybinding.h"

namespace interface {

UrlInput::UrlInput(const std::string& label, const std::string& success, const Callback& on_submit)
    : label_{label},
      success_{success},
      on_submit_{on_submit},
      input_{[](const std::string& character) {
        // URLs contain only printable ASCII characters (without spaces)
        return character.size() == 1 && std::isgraph(static_cast<unsigned char>(character.front()));
      }} {}

/* ********************************************************************************************** */

ftxui::Element UrlInput::Render() {
  using Keybind = keybinding::Navigation;

  // Hints are split in multiple lines to fit even on smaller terminals
  auto hint = ftxui::vbox({
                  ftxui::text(util::EventToString(Keybind::Return) + ": add"),
                  ftxui::text(util::EventToString(Keybind::Escape) + ": clear"),
              }) |
              ftxui::color(ftxui::Color::Grey82);

  ftxui::Element feedback = ftxui::text("");

  if (feedback_.has_value()) {
    feedback =
        feedback_->accepted
            ? ftxui::text("✓ " + feedback_->message) | ftxui::color(ftxui::Color::DarkSeaGreen2Bis)
            : ftxui::text("✗ " + feedback_->message) | ftxui::color(ftxui::Color::MistyRose1);
    feedback |= ftxui::bold;
  }

  auto padding = ftxui::text(std::string(kPadding, ' '));

  auto content = ftxui::vbox({
      ftxui::text(""),
      ftxui::text(label_) | ftxui::color(ftxui::Color::Grey93),
      ftxui::text(""),
      input_.Render(max_columns_ - 2 * kPadding, IsFocused()),
      ftxui::text(""),
      hint,
      ftxui::text(""),
      feedback,
  });

  return ftxui::hbox({padding, content | ftxui::xflex, padding}) | ftxui::reflect(Box());
}

/* ********************************************************************************************** */

bool UrlInput::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (event == Keybind::Return) {
    Submit();
    return true;
  }

  // Clear input, or let owner handle it (e.g. to close dialog) when there is nothing to clear
  if (event == Keybind::Escape) {
    if (input_.IsEmpty() && !feedback_.has_value()) return false;

    Clear();
    return true;
  }

  // Every character belongs to the URL (none of them should trigger any other keybinding)
  if (input_.OnEvent(event)) {
    if (event.is_character()) feedback_.reset();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void UrlInput::Clear() {
  input_.Clear();
  feedback_.reset();
}

/* ********************************************************************************************** */

/* ********************************************************************************************** */

void UrlInput::Submit() {
  if (input_.IsEmpty()) return;

  LOG("Submit URL=", std::quoted(input_.GetText()));
  auto error = on_submit_(input_.GetText());

  if (error.has_value()) {
    // Keep text, so user can fix it
    feedback_ = Feedback{.accepted = false, .message = *error};
    return;
  }

  input_.Clear();
  feedback_ = Feedback{.accepted = true, .message = success_};
}

}  // namespace interface
