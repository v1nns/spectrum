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

            model::Song new_song{.filepath = *active};

            modified_playlist_->songs.emplace_back(new_song);
            menu_playlist_->Emplace(new_song);
            btn_save_->Enable();

            return true;
          },
          menu::Style::Alternative)),

      input_playlist_(ftxui::Input(&input_name_,
                                   ftxui::InputOption{
                                       .multiline = false,
                                       .cursor_position = &cursor_position_,
                                   })),

      menu_playlist_(menu::CreateSongMenu(
          dispatcher,

          // Callback to force a UI refresh
          [this] {
            auto disp = dispatcher_.lock();
            if (!disp) return;

            disp->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this](const std::optional<model::Song>& active) {
            if (!active) return false;

            // Send user action to controller, try to play selected entry
            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return false;

            LOG("Handle on_click event on menu entry=", active->filepath.filename());

            if (!modified_playlist_.has_value() || modified_playlist_->IsEmpty()) {
              return false;
            }

            auto it = std::find_if(
                modified_playlist_->songs.begin(), modified_playlist_->songs.end(),
                [active](const model::Song& s) { return s.filepath == active->filepath; });

            bool found = it != modified_playlist_->songs.end();

            ERROR_IF(!found, "Could not find song in modified playlist");

            if (found) {
              // Erase from structures
              modified_playlist_->songs.erase(it);
              menu_playlist_->Erase(*active);

              UpdateButtonState();
            }

            return true;
          })) {
  CreateButtons();

  // Append all inner elements to have focus controlled by wrapper
  focus_ctl_.Append(*menu_files_.get(), *menu_playlist_.get());

  focus_ctl_.SetFocus(0);
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

  std::vector<model::Song> songs;

  switch (curr_operation_.action) {
    case model::PlaylistOperation::Operation::None:
    case model::PlaylistOperation::Operation::Create:
      // Making sure that playlist is empty
      curr_operation_.playlist = model::Playlist{.index = -1};
      break;

    case model::PlaylistOperation::Operation::Modify:
      if (const auto& playlist = curr_operation_.playlist; !playlist->IsEmpty()) {
        songs.reserve(playlist->songs.size());

        for (const auto& song : playlist->songs) {
          songs.emplace_back(song);
        }
      }
      break;

    case model::PlaylistOperation::Operation::Delete:
      break;
  }

  modified_playlist_ = curr_operation_.playlist;

  input_name_ = !modified_playlist_->name.empty() ? modified_playlist_->name : "<unnamed>";
  menu_playlist_->SetEntries(songs);

  Dialog::Open();
}

/* ********************************************************************************************** */

ftxui::Element PlaylistDialog::RenderImpl(const ftxui::Dimensions& curr_size) const {
  int max_columns_per_menu = ((curr_size.dimx * 0.5f) / 2);
  int max_lines_menu = (curr_size.dimy * 0.6f);

  menu_files_->SetMaxColumns(max_columns_per_menu);
  menu_playlist_->SetMaxColumns(max_columns_per_menu);

  std::string title;

  switch (curr_operation_.action) {
    case model::PlaylistOperation::Operation::None:
    case model::PlaylistOperation::Operation::Create:
      title = "Create Playlist";
      break;
    case model::PlaylistOperation::Operation::Modify:
      title = "Modify Playlist";
      break;
    case model::PlaylistOperation::Operation::Delete:
      title = "Delete Playlist";  // ???
      break;
  }

  auto size_decorator = ftxui::size(ftxui::WIDTH, ftxui::EQUAL, max_columns_per_menu) |
                        ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, max_lines_menu);

  int playlist_width = max_columns_per_menu * 0.7f;
  auto playlist_input = edit_mode_ ? input_playlist_->Render() : ftxui::text(input_name_);

  auto playlist_decorator =
      ftxui::size(ftxui::WIDTH, ftxui::EQUAL, std::min(playlist_width, (int)input_name_.length()));

  return ftxui::vbox({
             ftxui::text(" "),
             ftxui::text(title) | ftxui::color(ftxui::Color::Black) | ftxui::center | ftxui::bold,
             ftxui::text(" "),

             ftxui::hbox({
                 ftxui::filler(),

                 ftxui::vbox({
                     ftxui::filler(),
                     // Using hbox as title, otherwise color will be applied incorrectly on border
                     ftxui::window(
                         ftxui::hbox({ftxui::text(" files ") | ftxui::color(ftxui::Color::Grey11)}),
                         menu_files_->Render()) |
                         size_decorator |
                         (menu_files_->IsFocused() ? ftxui::color(ftxui::Color::LightSkyBlue1)
                                                   : ftxui::color(ftxui::Color::Grey11)),
                     ftxui::filler(),
                 }),

                 ftxui::filler(),

                 ftxui::vbox({
                     ftxui::filler(),
                     ftxui::window(ftxui::hbox({ftxui::text(" "),
                                                playlist_input | playlist_decorator |
                                                    ftxui::color(ftxui::Color::Grey11),
                                                ftxui::text(" ")}),
                                   menu_playlist_->Render()) |
                         size_decorator |
                         (menu_playlist_->IsFocused() ? ftxui::color(ftxui::Color::LightSkyBlue1)
                                                      : ftxui::color(ftxui::Color::Grey11)),
                     ftxui::filler(),

                 }),

                 ftxui::filler(),

             }) | ftxui::center |
                 ftxui::flex_grow,

             ftxui::filler(),
             btn_save_->Render() | ftxui::center,
             ftxui::filler(),
         }) |
         ftxui::flex_grow;
}

/* ********************************************************************************************** */

bool PlaylistDialog::OnEventImpl(const ftxui::Event& event) {
  // Menu should handle first
  if (menu_files_->IsFocused()) {
    // Filter "return" event to not be handled by menu
    if (event == keybinding::Navigation::Return) return true;

    if (menu_files_->OnEvent(event)) return true;

    if (event == keybinding::Navigation::Space) {
      LOG("Handle key to add file to playlist");
      menu_files_->OnClick();
      return true;
    }
  }

  if (menu_playlist_->IsFocused()) {
    if (edit_mode_) {
      // Exit from edit mode
      if (event == keybinding::Navigation::Return || event == keybinding::Navigation::Escape) {
        edit_mode_ = false;
        modified_playlist_->name = input_name_;
        UpdateButtonState();

        return true;
      }

      // Otherwise, input will handle any event
      return input_playlist_->OnEvent(event);
    }

    // Filter "return" event to not be handled by menu
    if (event == keybinding::Navigation::Return) return true;

    if (menu_playlist_->OnEvent(event)) return true;

    if (event == keybinding::Playlist::Rename) {
      LOG("Handle key to rename playlist");
      edit_mode_ = true;
      return true;
    }

    if (event == keybinding::Navigation::Space) {
      LOG("Handle key to remove file from playlist");
      menu_playlist_->OnClick();
      return true;
    }
  }

  // Otherwise, pass event to focus controller to handle and pass it along to focused element
  if (focus_ctl_.OnEvent(event)) {
    return true;
  }

  if (modified_playlist_.has_value() && btn_save_->IsActive() &&
      event == keybinding::Playlist::Save) {
    LOG("Handle key to save playlist");
    btn_save_->OnClick();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool PlaylistDialog::OnMouseEventImpl(ftxui::Event event) {
  if (focus_ctl_.OnMouseEvent(event)) return true;

  if (btn_save_->IsActive() && btn_save_->OnMouseEvent(event)) return true;

  return false;
}

/* ********************************************************************************************** */

void PlaylistDialog::OnOpen() {
  // do nothing
}

/* ********************************************************************************************** */

void PlaylistDialog::OnClose() {
  // Reset default path in file menu
  menu_files_->actual().RefreshList(std::filesystem::current_path());

  modified_playlist_.reset();
  btn_save_->Disable();
  cursor_position_ = 0;
  edit_mode_ = false;

  focus_ctl_.SetFocus(0);
}

/* ********************************************************************************************** */

void PlaylistDialog::CreateButtons() {
  // Style for save button
  auto style = Button::Style{
      .normal =
          Button::Style::State{
              .foreground = ftxui::Color::Black,
              .border = ftxui::Color::GrayDark,
          },

      .focused = Button::Style::State{.border = ftxui::Color::SteelBlue3},
      .width = 16,
  };

  btn_save_ = Button::make_button(
      std::string("Save"),
      [this]() {
        LOG("Handle callback for Playlist save button");
        if (modified_playlist_.has_value()) {
          auto dispatcher = dispatcher_.lock();
          if (!dispatcher) return false;

          LOG("Sending modified playlist to be saved, playlist=", *modified_playlist_);
          auto event_save = interface::CustomEvent::SavePlaylistsToFile(*modified_playlist_);
          dispatcher->SendEvent(event_save);

          // Update UI state
          curr_operation_.playlist = modified_playlist_;
          UpdateButtonState();
        }

        return true;
      },
      style, false);
}

/* ********************************************************************************************** */

void PlaylistDialog::UpdateButtonState() {
  if (modified_playlist_ == curr_operation_.playlist) {
    btn_save_->Disable();
  } else {
    btn_save_->Enable();
  }
}
}  // namespace interface
