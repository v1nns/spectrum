#include "view/element/playlist_dialog.h"

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"

namespace interface {

PlaylistDialog::PlaylistDialog()
    : Dialog(Size{.width = 0.6f, .height = 0.8f, .min_column = kMinColumns, .min_line = kMinLines},
             // TODO: change colors
             Style{.background = ftxui::Color::SteelBlue3, .foreground = ftxui::Color::Grey11}) {
  CreateButtons();
}

/* ********************************************************************************************** */

ftxui::Element PlaylistDialog::RenderImpl() const {
  // TODO: implement
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

void PlaylistDialog::OnOpen() {
  if (std::string home{file_handler_.GetHome()}; !file_handler_.ListFiles(home, entries_)) {
    // TODO: log error
  }
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
