#include "view/element/playlist_dialog.h"

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace interface {

PlaylistDialog::PlaylistDialog()
    : Dialog(Size{.width = 0.6f, .height = 0.8f, .min_column = kMinColumns, .min_line = kMinLines},
             Style{.background = ftxui::Color::SteelBlue, .foreground = ftxui::Color::Grey93}) {
  CreateButtons();
}

/* ********************************************************************************************** */

void PlaylistDialog::Open(const model::PlaylistOperation& operation) {
  curr_operation_ = operation;
  Dialog::Open();
}

/* ********************************************************************************************** */

ftxui::Element PlaylistDialog::RenderImpl() const {
  // TODO: implement
  switch (curr_operation_.action) {
    case model::PlaylistOperation::Operation::None:
      break;
    case model::PlaylistOperation::Operation::Create:
      break;
    case model::PlaylistOperation::Operation::Modify:
      break;
    case model::PlaylistOperation::Operation::Delete:
      break;
  }
  return ftxui::vbox({
      ftxui::text(" "),
      ftxui::text("Manage Playlist") | ftxui::color(ftxui::Color::Black) | ftxui::center |
          ftxui::bold,
      ftxui::hbox({
          ftxui::text("  "),
          ftxui::vbox({
              ftxui::text(" "),
              ftxui::window(ftxui::hbox({ftxui::text(" files ") |
                                         ftxui::color(ftxui::Color::PaleTurquoise1)}),
                            ftxui::vbox({
                                ftxui::text("entry 1") | ftxui::color(ftxui::Color::Black),
                                ftxui::text("entry 2"),
                                ftxui::text("entry 3"),
                                ftxui::text("entry 4"),
                                ftxui::text("entry 5"),
                                ftxui::text("entry 6"),
                                ftxui::text("entry 7"),
                                ftxui::text("entry 8"),
                            })) |
                  ftxui::flex_grow,
          }) | ftxui::flex_grow,
          ftxui::vbox({
              ftxui::filler(),
              btn_add_->Render(),
              ftxui::text(""),
              btn_remove_->Render(),
              ftxui::filler(),
          }),
          ftxui::vbox({
              ftxui::text(" "),
              ftxui::window(
                  ftxui::hbox({ftxui::text(" name ") | ftxui::color(ftxui::Color::PaleTurquoise1)}),
                  ftxui::vbox({
                      ftxui::text("entry 1"),
                      ftxui::text("entry 2"),
                      ftxui::text("entry 4"),
                      ftxui::text("entry 7"),
                      ftxui::text("entry 8"),
                  })) |
                  ftxui::flex_grow,
          }) | ftxui::flex_grow,
          ftxui::text("  "),
      }) | ftxui::flex_grow,
  });
}

/* ********************************************************************************************** */

bool PlaylistDialog::OnEventImpl(const ftxui::Event& event) {
  if (btn_add_->OnMouseEvent(event)) return true;

  if (btn_remove_->OnMouseEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

void PlaylistDialog::CreateButtons() {
  auto style = Button::Style{
      .normal =
          Button::Style::State{
              .foreground = ftxui::Color::Black,
              .border = ftxui::Color::Black,
          },

      .decorator = ftxui::bold | ftxui::color(ftxui::Color::PaleTurquoise1),
      .width = 3,
  };

  btn_add_ = Button::make_button(
      std::string(">"),
      [this]() {
        // TODO: do something
        return true;
      },
      style);

  btn_remove_ = Button::make_button(
      std::string("<"),
      [this]() {
        // TODO: do something
        return true;
      },
      style);
}

}  // namespace interface
