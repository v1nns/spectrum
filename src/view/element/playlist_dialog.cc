#include "view/element/playlist_dialog.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/string.hpp"
#include "model/playlist_operation.h"
#include "util/formatter.h"
#include "util/url.h"

namespace interface {

namespace {

//! Remove spaces from both ends of the given text
std::string Trim(const std::string& text) {
  static constexpr std::string_view kSpaces = " \t";

  auto first = text.find_first_not_of(kSpaces);
  if (first == std::string::npos) return "";

  return text.substr(first, text.find_last_not_of(kSpaces) - first + 1);
}

}  // namespace

/* ********************************************************************************************** */

PlaylistDialog::PlaylistDialog(const std::shared_ptr<EventDispatcher>& dispatcher,
                               const std::function<bool(const util::File& file)>& contains_audio_cb,
                               const std::string& optional_path,
                               const std::function<bool()>& stream_available_cb,
                               const PlaylistFetchCallback& fetch_playlist_cb)
    : Dialog(dispatcher,
             Size{.width = 0.6f, .height = 0.8f, .min_column = kMinColumns, .min_line = kMinLines},
             Style{.background = ftxui::Color::SteelBlue, .foreground = ftxui::Color::Grey93}),
      base_path_(),
      stream_available_cb_(stream_available_cb),
      fetch_playlist_cb_(fetch_playlist_cb),
      menu_files_(menu::CreateFileMenu(
          dispatcher, std::make_shared<util::FileHandler>(),

          // Callback to force a UI refresh
          [this] {
            auto dispatcher = GetDispatcher();
            if (!dispatcher) return;

            dispatcher->SendEvent(interface::CustomEvent::Refresh());
          },

          // Callback triggered on menu item click
          [this, contains_audio_cb](const std::optional<util::File>& active) {
            if (!active) return false;

            // Send user action to controller, try to play selected entry
            auto dispatcher = GetDispatcher();
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

      menu_playlist_(menu::CreateSongMenu(
          dispatcher,

          // Callback to force a UI refresh
          [this] {
            auto dispatcher = GetDispatcher();
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

            auto it = std::find_if(modified_playlist_->songs.begin(),
                                   modified_playlist_->songs.end(), [active](const model::Song& s) {
                                     return s.index == active->index && s.Compare(*active);
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
          })),

      message_{[this] {
                 // Message has expired, so UI must be refreshed to remove it from screen
                 if (auto dispatcher = GetDispatcher(); dispatcher) {
                   dispatcher->SendEvent(interface::CustomEvent::Refresh());
                 }
               },
               kMessageDuration} {
  url_input_ =
      std::make_unique<UrlInput>(std::string{kUrlLabel}, "Added to playlist",
                                 [this](const std::string& url) { return SubmitUrl(url); });

  CreateButtons();

  // Append all inner elements to have focus controlled by wrapper
  focus_ctl_.Append(*menu_files_.get(), *menu_playlist_.get());
  ShowSource(Source::Files);

  // Set default path to list files from
  base_path_ = menu_files_->actual().GetCurrentDir();
}

/* ********************************************************************************************** */

PlaylistDialog::~PlaylistDialog() { StopImport(); }

/* ********************************************************************************************** */

void PlaylistDialog::Open(const model::PlaylistOperation& operation) {
  // Always read files from default path again, as they may have changed since last time
  menu_files_->actual().RefreshList(base_path_);

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
  rename_.editing = false;
  rename_.error.reset();
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

  // Left pane shows songs to add from the active source (files or YouTube URL)
  bool source_focused = menu_files_->IsFocused() || url_input_->IsFocused();
  url_input_->SetMaxColumns(max_columns_per_menu - kPrefixOffset);

  ftxui::Element source_view =
      source_ == Source::Files ? menu_files_->Render() : url_input_->Render();

  // Tab buttons are rendered in bold while pane is focused
  btn_files_->UpdateParentFocus(source_focused);
  btn_youtube_->UpdateParentFocus(source_focused);

  // Message is drawn next to save button, and the same width is reserved on the other side to
  // keep button centered
  auto [message, message_style] = GetSaveMessage();
  if (!message.empty()) message = " " + message;
  int message_width = ftxui::string_width(message);

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
                     ftxui::window(ftxui::hbox({btn_files_->Render(), btn_youtube_->Render()}),
                                   source_view) |
                         size_decorator | focus_decorator(source_focused),
                     ftxui::filler(),
                 }),

                 ftxui::filler(),

                 ftxui::vbox({
                     ftxui::filler(),
                     ftxui::window(RenderPlaylistTitle(max_columns_per_menu),
                                   menu_playlist_->Render()) |
                         size_decorator | focus_decorator(menu_playlist_->IsFocused()),
                     ftxui::filler(),

                 }),

                 ftxui::filler(),

             }) | ftxui::center |
                 ftxui::flex_grow,

             ftxui::filler(),
             ftxui::hbox({
                 ftxui::filler(),
                 ftxui::text(std::string(message_width, ' ')),
                 btn_save_->Render(),
                 ftxui::text(message) | message_style | ftxui::vcenter,
                 ftxui::filler(),
             }),
             ftxui::filler(),
         }) |
         ftxui::flex_grow;
}

/* ********************************************************************************************** */

bool PlaylistDialog::OnEventImpl(const ftxui::Event& event) {
  // Playlist import finishes in another thread, which asks for a refresh (received as an event)
  FinishImport();

  // Text input should handle first, as it takes all characters
  if (url_input_->IsFocused() && url_input_->OnEvent(event)) return true;

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
    if (rename_.editing && OnRenameEvent(event)) return true;

    if (menu_playlist_->OnEvent(event)) return true;

    if (event == keybinding::Playlist::Rename) {
      LOG("Handle key to rename playlist");
      StartRename();
      return true;
    }

    if (event == keybinding::Navigation::Space) {
      LOG("Handle key to remove file from playlist");
      menu_playlist_->OnClick();
      return true;
    }
  }

  // Switch focus between menus (unless playlist name is being edited)
  if (!rename_.editing) {
    if (event == keybinding::Playlist::ShowFiles) {
      LOG("Handle key to show files");
      ShowSource(Source::Files);
      return true;
    }

    if (event == keybinding::Playlist::ShowYoutube) {
      LOG("Handle key to show YouTube URL input");
      ShowSource(Source::Youtube);
      return true;
    }

    if (event == keybinding::Navigation::Tab) {
      LOG("Handle key to focus next menu");
      focus_ctl_.FocusNext();
      return true;
    }

    if (event == keybinding::Navigation::TabReverse) {
      LOG("Handle key to focus previous menu");
      focus_ctl_.FocusPrevious();
      return true;
    }
  }

  // Let base class close this dialog, instead of focus controller removing focus from menu
  if (event == keybinding::Navigation::Escape) return false;

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
  if (btn_files_->OnMouseEvent(event)) return true;
  if (btn_youtube_->OnMouseEvent(event)) return true;

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
  StopImport();
  modified_playlist_.reset();
  rename_.editing = false;
  rename_.error.reset();
  btn_save_->Disable();
  message_.Hide();

  url_input_->Clear();
  ShowSource(Source::Files);
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

      .pressed =
          Button::Style::State{
              .foreground = ftxui::Color::SteelBlue3,
              .background = ftxui::Color::LightSteelBlue3,
              .border = ftxui::Color::SteelBlue3,
          },

      .disabled =
          Button::Style::State{
              .foreground = ftxui::Color::Grey35,
              .background = ftxui::Color::SteelBlue,
              .border = ftxui::Color::GrayDark,
          },

      .width = 16,
  };

  btn_save_ = Button::make_button_solid(
      std::string("Save"),
      [this]() {
        LOG("Handle callback for Playlist save button");
        if (modified_playlist_.has_value() && !modified_playlist_->name.empty() &&
            !modified_playlist_->IsEmpty()) {
          auto dispatcher = GetDispatcher();
          if (!dispatcher) return false;

          LOG("Sending modified playlist to be saved, playlist=", *modified_playlist_);
          auto event_save = interface::CustomEvent::SavePlaylistsToFile(*modified_playlist_);
          dispatcher->SendEvent(event_save);

          // Let user know that playlist was saved
          message_.Show("Saved ✓");

          // Update UI state
          curr_operation_.playlist = modified_playlist_;
          UpdateButtonState();
        }

        return true;
      },
      style, false);

  // Style for tab buttons (on the left pane border)
  auto tab_style = Button::Style{
      .normal =
          Button::Style::State{
              .foreground = ftxui::Color::Grey11,
              .background = ftxui::Color::SteelBlue,
          },

      .focused =
          Button::Style::State{
              .foreground = ftxui::Color::Grey11,
              .background = ftxui::Color::LightSkyBlue1,
          },

      .selected =
          Button::Style::State{
              .foreground = ftxui::Color::Grey11,
              .background = ftxui::Color::LightSkyBlue1,
          },

      .delimiters = Button::Delimiters{" ", " "},
  };

  btn_files_ = Button::make_button_for_window(
      util::EventToString(keybinding::Playlist::ShowFiles) + ":files",
      [this]() {
        LOG("Handle callback for files tab button");
        ShowSource(Source::Files);
        return true;
      },
      tab_style);

  btn_youtube_ = Button::make_button_for_window(
      util::EventToString(keybinding::Playlist::ShowYoutube) + ":youtube",
      [this]() {
        LOG("Handle callback for YouTube tab button");
        ShowSource(Source::Youtube);
        return true;
      },
      tab_style);
}

/* ********************************************************************************************** */

void PlaylistDialog::StartRename() {
  rename_.input.SetText(modified_playlist_->name);
  rename_.editing = true;
  rename_.error.reset();
}

/* ********************************************************************************************** */

bool PlaylistDialog::OnRenameEvent(const ftxui::Event& event) {
  if (event == keybinding::Navigation::Return) {
    FinishRename(true);
    return true;
  }

  if (event == keybinding::Navigation::Escape) {
    LOG("Cancel rename, keeping name=", std::quoted(modified_playlist_->name));
    FinishRename(false);
    return true;
  }

  if (rename_.input.OnEvent(event)) {
    // Any change to the typed name makes the last error obsolete
    rename_.error.reset();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void PlaylistDialog::FinishRename(bool keep_name) {
  if (keep_name) {
    std::string name = Trim(rename_.input.GetText());
    const auto& other_names = curr_operation_.other_names;

    if (name.empty()) {
      rename_.error = "Name can't be empty";
      return;
    }

    if (std::find(other_names.begin(), other_names.end(), name) != other_names.end()) {
      rename_.error = "Name already used";
      return;
    }

    LOG("Rename playlist from ", std::quoted(modified_playlist_->name), " to ", std::quoted(name));
    modified_playlist_->name = name;
    UpdateButtonState();
  }

  rename_.editing = false;
  rename_.error.reset();
}

/* ********************************************************************************************** */

ftxui::Element PlaylistDialog::RenderPlaylistTitle(int max_columns) const {
  // Hint for the next possible action on playlist name
  std::string hint;

  if (rename_.editing) {
    hint = "[" + util::EventToString(keybinding::Navigation::Escape) + ":cancel]";
  } else if (menu_playlist_->IsFocused()) {
    hint = "[" + util::EventToString(keybinding::Playlist::Rename) + ":rename]";
  }

  std::string name = modified_playlist_.has_value() ? modified_playlist_->name : "";
  if (name.empty()) name = kUnnamed;

  // Border is split into: corner, space, title, space, line (at least one column), hint, corner
  static constexpr int kFixedColumns = 4;
  int hint_columns = hint.empty() ? 0 : static_cast<int>(hint.size()) + 1;
  int title_columns = max_columns - kFixedColumns - hint_columns;

  // Prefer to show title instead of hint, when there is not enough space for both (while not
  // editing, the whole name should fit)
  int required =
      rename_.editing ? kMinTitleColumns : std::max(kMinTitleColumns, ftxui::string_width(name));

  if (title_columns < required) {
    title_columns += hint_columns;
    hint.clear();
  }

  ftxui::Element title =
      rename_.editing ? rename_.input.Render(title_columns, true, std::string(kNamePlaceholder))
                      : ftxui::text(name) | ftxui::color(ftxui::Color::Grey11) |
                            ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, title_columns);

  return ftxui::hbox({
      ftxui::text(" "),
      title,
      ftxui::text(" "),
      ftxui::filler(),
      ftxui::text(hint) | ftxui::color(ftxui::Color::Grey82),
  });
}

/* ********************************************************************************************** */

std::pair<std::string, ftxui::Decorator> PlaylistDialog::GetSaveMessage() const {
  // Error from last attempt to rename playlist
  if (rename_.editing && rename_.error.has_value()) {
    return {"✗ " + *rename_.error, ftxui::color(ftxui::Color::MistyRose1) | ftxui::bold};
  }

  // Confirmation after saving playlist
  if (auto text = message_.GetText(); text.has_value()) {
    return {*text, ftxui::bold};
  }

  // Reason why playlist cannot be saved yet
  if (!rename_.editing && modified_playlist_.has_value() && !btn_save_->IsActive()) {
    auto style = ftxui::color(ftxui::Color::Grey82);

    if (modified_playlist_->IsEmpty()) return {"Add a song to save", style};

    if (modified_playlist_->name.empty()) {
      return {"Name it to save (" + util::EventToString(keybinding::Playlist::Rename) + ")", style};
    }
  }

  return {"", ftxui::nothing};
}

/* ********************************************************************************************** */

void PlaylistDialog::ShowSource(Source source) {
  auto get_view = [this](Source s) -> Element& {
    if (s == Source::Files) return *menu_files_;
    return *url_input_;
  };

  // Replace view on the left pane and focus it
  focus_ctl_.Replace(get_view(source_), get_view(source));
  focus_ctl_.SetFocus(kSourcePane);

  source_ = source;

  // Let user know when songs cannot be added from URL (before typing anything)
  if (source == Source::Youtube) {
    url_input_->SetLabel(IsStreamAvailable() ? std::string{kUrlLabel} : std::string{kNoUrlLabel});
  }

  // Update tab buttons
  bool show_files = source == Source::Files;
  show_files ? btn_files_->Select() : btn_files_->Unselect();
  show_files ? btn_youtube_->Unselect() : btn_youtube_->Select();
}

/* ********************************************************************************************** */

std::optional<std::string> PlaylistDialog::AddUrl(const std::string& url) {
  if (!modified_playlist_.has_value()) return "No playlist to add song to";

  if (!util::IsYoutubeUrl(url)) return "Not a YouTube URL";

  if (!IsStreamAvailable()) return "yt-dlp not found";

  model::Song new_song{
      .index = static_cast<int>(modified_playlist_->songs.size()),
      .stream_info = model::StreamInfo{.base_url = url},
  };

  bool duplicated =
      std::any_of(modified_playlist_->songs.begin(), modified_playlist_->songs.end(),
                  [&new_song](const model::Song& song) { return song.Compare(new_song); });

  if (duplicated) return "Already in playlist";

  LOG("Adding new song from URL=", std::quoted(url),
      " to modified playlist=", std::quoted(modified_playlist_->name));

  modified_playlist_->songs.emplace_back(new_song);
  menu_playlist_->Emplace(new_song);

  UpdateButtonState();

  return std::nullopt;
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

/* ********************************************************************************************** */

UrlInput::Result PlaylistDialog::SubmitUrl(const std::string& url) {
  using Status = UrlInput::Result::Status;

  if (!util::IsYoutubePlaylistUrl(url)) {
    auto error = AddUrl(url);
    return error ? UrlInput::Result{Status::Rejected, *error}
                 : UrlInput::Result{Status::Accepted, ""};
  }

  if (!modified_playlist_.has_value()) return {Status::Rejected, "No playlist to add songs to"};
  if (!fetch_playlist_cb_) return {Status::Rejected, "Cannot import playlist"};
  if (!IsStreamAvailable()) return {Status::Rejected, "yt-dlp not found"};

  if (import_.thread.joinable()) return {Status::Rejected, "Already importing"};

  StartImport(url);
  return {Status::Pending, "Importing playlist"};
}

/* ********************************************************************************************** */

void PlaylistDialog::StartImport(const std::string& url) {
  LOG("Start importing songs from playlist URL=", std::quoted(url));
  import_.cancel = false;

  import_.thread = std::thread([this, url] {
    util::Logger::SetThreadName("import");

    std::vector<model::Song> songs;
    error::Code result = fetch_playlist_cb_(url, songs, &import_.cancel);

    if (import_.cancel) return;

    {
      std::scoped_lock lock{import_.mutex};
      import_.result = std::make_pair(result, std::move(songs));
    }

    // Ask for a refresh, so result is added to playlist right away
    if (auto dispatcher = GetDispatcher(); dispatcher) {
      dispatcher->SendEvent(interface::CustomEvent::Refresh());
    }
  });
}

/* ********************************************************************************************** */

void PlaylistDialog::FinishImport() {
  std::optional<std::pair<error::Code, std::vector<model::Song>>> result;

  {
    std::scoped_lock lock{import_.mutex};
    if (!import_.result.has_value()) return;
    result.swap(import_.result);
  }

  // Thread has already finished its job
  if (import_.thread.joinable()) import_.thread.join();

  auto& [code, songs] = *result;
  using Status = UrlInput::Result::Status;

  if (code != error::kSuccess || !modified_playlist_.has_value()) {
    url_input_->SetResult({Status::Rejected, code == error::kStreamFetcherNotFound
                                                 ? "yt-dlp not found"
                                                 : "Cannot import playlist"});
    return;
  }

  int added = 0;
  int skipped = 0;

  for (auto& song : songs) {
    bool duplicated =
        std::any_of(modified_playlist_->songs.begin(), modified_playlist_->songs.end(),
                    [&song](const model::Song& other) { return other.Compare(song); });

    if (duplicated) {
      skipped++;
      continue;
    }

    song.index = static_cast<int>(modified_playlist_->songs.size());
    modified_playlist_->songs.emplace_back(song);
    menu_playlist_->Emplace(song);
    added++;
  }

  LOG("Imported songs from playlist, added=", added, " skipped=", skipped);
  UpdateButtonState();

  std::string message = "Added " + std::to_string(added) + (added == 1 ? " song" : " songs");
  if (skipped > 0) message += ", " + std::to_string(skipped) + " skipped";

  url_input_->SetResult({added > 0 ? Status::Accepted : Status::Rejected,
                         added > 0 ? message : "No new songs to add"});
}

/* ********************************************************************************************** */

void PlaylistDialog::StopImport() {
  if (!import_.thread.joinable()) return;

  LOG("Stop importing songs from playlist");
  import_.cancel = true;
  import_.thread.join();

  std::scoped_lock lock{import_.mutex};
  import_.result.reset();
}

}  // namespace interface
