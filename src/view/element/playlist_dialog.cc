#include "view/element/playlist_dialog.h"

#include <algorithm>
#include <filesystem>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/playlist_operation.h"

namespace interface {

PlaylistDialog::PlaylistDialog(const std::shared_ptr<EventDispatcher>& dispatcher,
                               const std::function<bool(const util::File& file)>& contains_audio_cb,
                               const std::string& optional_path)
    : Dialog(Size{.width = 0.6f, .height = 0.8f, .min_column = kMinColumns, .min_line = kMinLines},
             Style{.background = ftxui::Color::SteelBlue, .foreground = ftxui::Color::Grey93}),
      dispatcher_(dispatcher),
      base_path_(),
      menu_files_(menu::CreateFileMenu(
          dispatcher, std::make_shared<util::FileHandler>(),

          // Callback to force a UI refresh
          [this] {
            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return;

            dispatcher->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this, contains_audio_cb](const std::optional<util::File>& active) {
            if (!active) return false;

            // Send user action to controller, try to play selected entry
            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return false;

            LOG("Handle on_click event on menu entry=", *active);

            if (contains_audio_cb(*active)) {
              LOG("Adding new song=", std::quoted(active->filename().string()),
                  " to modified playlist=", std::quoted(modified_playlist_->name));
              model::Song new_song{.index = modified_playlist_->songs.size(), .filepath = *active};

              modified_playlist_->songs.emplace_back(new_song);
              menu_playlist_->Emplace(new_song);

              UpdateButtonState();
            }

            return true;
          },
          menu::Style::Alternative, optional_path)),

      input_playlist_(),

      menu_playlist_(menu::CreateSongMenu(
          dispatcher,

          // Callback to force a UI refresh
          [this] {
            auto dispatcher = dispatcher_.lock();
            if (!dispatcher) return;

            dispatcher->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this](const std::optional<model::Song>& active) {
            if (!active) return false;

            LOG("Handle on_click event on menu entry=", active->filepath.filename());

            if (!modified_playlist_.has_value() || modified_playlist_->IsEmpty()) {
              return false;
            }

            auto it =
                std::find_if(modified_playlist_->songs.begin(), modified_playlist_->songs.end(),
                             [active](const model::Song& s) {
                               return s.index == active->index && s.filepath == active->filepath;
                             });

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

  // Set default path to list files from
  base_path_ = menu_files_->actual().GetCurrentDir();
}

/* ********************************************************************************************** */

void PlaylistDialog::Open(const model::PlaylistOperation& operation) {
  // Check if FileMenu must reset list of files back to default path
  if (auto& derived = menu_files_->actual(); derived.GetCurrentDir() != base_path_) {
    derived.RefreshList(base_path_);
  }

  // Update internal cache
  curr_operation_ = operation;

  switch (curr_operation_.action) {
    case model::PlaylistOperation::Operation::None:
    case model::PlaylistOperation::Operation::Create:
      // Making sure that playlist is empty
      curr_operation_.playlist = model::Playlist{.index = -1};
      break;

    case model::PlaylistOperation::Operation::Modify:
      if (int i = 0; !curr_operation_.playlist->IsEmpty()) {
        for (auto& song : curr_operation_.playlist->songs) {
          song.index = i++;
        }
      }
      break;
  }

  modified_playlist_ = curr_operation_.playlist;

  // Clear playlist info
  input_playlist_.name = !modified_playlist_->name.empty() ? modified_playlist_->name : "";
  menu_playlist_->SetEntries(modified_playlist_->songs);

  UpdateButtonState();

  Dialog::Open();
}

/* ********************************************************************************************** */

ftxui::Element PlaylistDialog::RenderImpl(const ftxui::Dimensions& curr_size) const {
  static constexpr int kPrefixOffset = 2;

  int max_columns_per_menu = ((curr_size.dimx * 0.5f) / 2);
  int max_lines_menu = (curr_size.dimy * 0.6f);

  // Value is smaller here because of menu prefix
  menu_files_->SetMaxColumns(max_columns_per_menu - kPrefixOffset);
  menu_playlist_->SetMaxColumns(max_columns_per_menu - kPrefixOffset);

  std::string title;

  switch (curr_operation_.action) {
    case model::PlaylistOperation::Operation::None:
    case model::PlaylistOperation::Operation::Create:
      title = "Create Playlist";
      break;
    case model::PlaylistOperation::Operation::Modify:
      title = "Modify Playlist";
      break;
  }

  auto size_decorator = ftxui::size(ftxui::WIDTH, ftxui::EQUAL, max_columns_per_menu) |
                        ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, max_lines_menu);

  int playlist_width = max_columns_per_menu * 0.7f;
  auto playlist_input = input_playlist_.Render(playlist_width);

  constexpr auto focus_decorator = [](bool is_focused) {
    return is_focused ? ftxui::color(ftxui::Color::LightSkyBlue1)
                      : ftxui::color(ftxui::Color::Grey11);
  };

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
                         size_decorator | focus_decorator(menu_files_->IsFocused()),
                     ftxui::filler(),
                 }),

                 ftxui::filler(),

                 ftxui::vbox({
                     ftxui::filler(),
                     ftxui::window(
                         ftxui::hbox({ftxui::text(" "), playlist_input, ftxui::text(" ")}),
                         menu_playlist_->Render()) |
                         size_decorator | focus_decorator(menu_playlist_->IsFocused()),
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
    if (menu_files_->OnEvent(event)) return true;

    if (event == keybinding::Navigation::Space) {
      LOG("Handle key to add file to playlist");
      menu_files_->OnClick();
      return true;
    }
  }

  if (menu_playlist_->IsFocused()) {
    if (input_playlist_.IsEditing() && input_playlist_.OnEvent(event)) {
      modified_playlist_->name = input_playlist_.name;
      UpdateButtonState();

      return true;
    }

    if (menu_playlist_->OnEvent(event)) return true;

    if (event == keybinding::Playlist::Rename) {
      LOG("Handle key to rename playlist");
      input_playlist_.edit_mode = true;
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
  input_playlist_.Clear();
  btn_save_->Disable();

  focus_ctl_.SetFocus(0);
}

/* ********************************************************************************************** */

void PlaylistDialog::CreateButtons() {
  // Style for save button
  auto style = Button::Style{
      .normal =
          Button::Style::State{
              .foreground = ftxui::Color::Black,
              .background = ftxui::Color::SkyBlue3,
              .border = ftxui::Color::GrayDark,
          },

      .focused =
          Button::Style::State{
              .foreground = ftxui::Color::LightSkyBlue1,
              .background = ftxui::Color::DeepSkyBlue4Ter,
              .border = ftxui::Color::LightSkyBlue1,
          },

      .disabled =
          Button::Style::State{
              .foreground = ftxui::Color::Grey35,
              .background = ftxui::Color::SteelBlue,
          },

      .width = 16,
  };

  btn_save_ = Button::make_button_solid(
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
  // Check for a few conditions before enabling the save button
  if (!modified_playlist_->name.empty() && !modified_playlist_->IsEmpty() &&
      modified_playlist_ != curr_operation_.playlist) {
    btn_save_->Enable();
  } else {
    btn_save_->Disable();
  }
}

}  // namespace interface
