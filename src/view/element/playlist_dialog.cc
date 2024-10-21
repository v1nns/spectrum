#include "view/element/playlist_dialog.h"

#include <filesystem>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/playlist_operation.h"

namespace interface {

PlaylistDialog::PlaylistDialog(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Dialog(Size{.width = 0.6f, .height = 0.8f, .min_column = kMinColumns, .min_line = kMinLines},
             Style{.background = ftxui::Color::SteelBlue, .foreground = ftxui::Color::Grey93}),
      dispatcher_(dispatcher),
      menu_files_(menu::CreateFileMenu(
          dispatcher, file_handler_,

          // Callback to force a UI refresh
          [this] {
            auto disp = dispatcher_.lock();
            if (!disp) return;

            disp->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this](const std::optional<util::File>& active) {
            if (!active) return false;

            // Send user action to controller, try to play selected entry
            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return false;

            LOG("Handle on_click event on menu entry=", *active);
            // TODO: implement
            return false;
          })),

      menu_playlist_(menu::CreateTextMenu(
          dispatcher,

          // Callback to force a UI refresh
          [this] {
            auto disp = dispatcher_.lock();
            if (!disp) return;

            disp->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this](const std::optional<std::string>& active) {
            if (!active) return false;

            // Send user action to controller, try to play selected entry
            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return false;

            LOG("Handle on_click event on menu entry=", *active);
            // TODO: implement
            return false;
          })) {
  CreateButtons();
}

/* ********************************************************************************************** */

void PlaylistDialog::Open(const model::PlaylistOperation& operation) {
  static std::filesystem::path curr_path = std::filesystem::current_path();

  // Check if FileMenu must reset list of files back to default path
  if (auto& derived = menu_files_->actual(); derived.GetCurrentDir() != curr_path) {
    derived.RefreshList(curr_path);
  }

  // Update internal cache
  curr_operation_ = operation;

  switch (curr_operation_.action) {
    case model::PlaylistOperation::Operation::None:
    case model::PlaylistOperation::Operation::Create:
      // Making sure that playlist is empty
      curr_operation_.playlist = model::Playlist{};
      break;

    case model::PlaylistOperation::Operation::Modify:
      if (const auto& playlist = curr_operation_.playlist; !playlist->IsEmpty()) {
        std::vector<std::string> songs;
        songs.reserve(playlist->songs.size());

        for (const auto& song : playlist->songs) {
          songs.push_back(song.filepath.filename().string());
        }

        menu_playlist_->SetEntries(songs);
      }
      break;

    case model::PlaylistOperation::Operation::Delete:
      break;
  }

  Dialog::Open();
}

/* ********************************************************************************************** */

ftxui::Element PlaylistDialog::RenderImpl(const ftxui::Dimensions& curr_size) const {
  int max_columns_per_menu = ((curr_size.dimx * 0.6f) / 2) - 9;
  menu_files_->SetMaxColumns(max_columns_per_menu);

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

  auto menu_decorator = ftxui::size(ftxui::WIDTH, ftxui::EQUAL, max_columns_per_menu) |
                        ftxui::color(ftxui::Color::Grey11);

  std::string playlist_name =
      !curr_operation_.playlist->name.empty() ? curr_operation_.playlist->name : "<unnamed>";

  return ftxui::vbox({
      ftxui::text(" "),
      ftxui::text("Manage Playlist") | ftxui::color(ftxui::Color::Black) | ftxui::center |
          ftxui::bold,
      ftxui::text(" "),

      ftxui::hbox({
          ftxui::text(" "),
          ftxui::filler(),

          ftxui::vbox({
              ftxui::text(" "),
              // Using hbox as title, otherwise color will be applied incorrectly on border
              ftxui::window(ftxui::hbox({
                                ftxui::text(" files ") | ftxui::color(ftxui::Color::PaleTurquoise1),
                            }),
                            menu_files_->Render()) |
                  menu_decorator,
          }) | ftxui::flex_grow,

          ftxui::filler(),

          ftxui::vbox({
              ftxui::filler(),
              btn_add_->Render(),
              ftxui::text(""),
              btn_remove_->Render(),
              ftxui::filler(),
          }),

          ftxui::filler(),

          ftxui::vbox({
              ftxui::text(" "),
              ftxui::window(ftxui::hbox({ftxui::text(" " + playlist_name + " ") |
                                         ftxui::color(ftxui::Color::PaleTurquoise1)}),
                            menu_playlist_->Render()) |
                  ftxui::flex_grow | menu_decorator,
          }) | ftxui::flex_grow,

          ftxui::filler(),
          ftxui::text(" "),

      }) | ftxui::flex_grow,

      ftxui::text(" "),
  });
}

/* ********************************************************************************************** */

bool PlaylistDialog::OnEventImpl(const ftxui::Event& event) {
  if (menu_files_->OnEvent(event)) return true;
  if (menu_playlist_->OnEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

bool PlaylistDialog::OnMouseEventImpl(ftxui::Event event) {
  if (menu_files_->OnMouseEvent(event)) return true;
  if (menu_playlist_->OnMouseEvent(event)) return true;

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
        return false;
      },
      style);

  btn_remove_ = Button::make_button(
      std::string("<"),
      [this]() {
        // TODO: do something
        return false;
      },
      style);
}

}  // namespace interface
