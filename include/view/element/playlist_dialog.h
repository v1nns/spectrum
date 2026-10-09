/**
 * \file
 * \brief  Class for dialog to manage a playlist
 */

#ifndef INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_

#include <atomic>
#include <chrono>
#include <ftxui/component/component_base.hpp>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "ftxui/component/screen_interactive.hpp"
#include "model/application_error.h"
#include "model/playlist_operation.h"
#include "model/song.h"
#include "util/file_handler.h"
#include "view/base/dialog.h"
#include "view/base/event_dispatcher.h"
#include "view/element/button.h"
#include "view/element/flash_message.h"
#include "view/element/focus_controller.h"
#include "view/element/menu.h"
#include "view/element/text_input.h"
#include "view/element/url_input.h"

namespace interface {

/**
 * @brief Customized dialog box to manage a single playlist
 */
class PlaylistDialog : public Dialog {
  static constexpr int kMinColumns = 45;  //!< Minimum columns for Element
  static constexpr int kMinLines = 25;    //!< Minimum lines for Element

  static constexpr std::chrono::milliseconds kMessageDuration{2000};  //!< Time to show message

  static constexpr std::string_view kUrlLabel = "Paste a YouTube URL:";  //!< Label for URL input
  static constexpr std::string_view kNoUrlLabel =
      "yt-dlp not installed";  //!< Label when URL cannot be added

  static constexpr int kSourcePane = 0;    //!< Focus index for pane with songs to add (files/URL)
  static constexpr int kPlaylistPane = 1;  //!< Focus index for pane with songs from playlist

  static constexpr std::string_view kUnnamed = "<unnamed>";  //!< Title for playlist without name
  static constexpr std::string_view kNamePlaceholder = "type a name";  //!< Placeholder for rename

  //! Minimum columns for playlist title (rename hint is hidden when there is less space)
  static constexpr int kMinTitleColumns = 10;

  //! Source of songs to add into playlist (displayed as tabs on the left pane)
  enum class Source {
    Files,    //!< Songs from local files
    Youtube,  //!< Songs streamed from YouTube URL
  };

 public:
  //! Callback to extract list of songs from a playlist URL (cancel flag is set to stop extraction)
  using PlaylistFetchCallback = std::function<error::Code(
      const std::string& url, std::vector<model::Song>& songs, const std::atomic<bool>* cancel)>;

  /**
   * @brief Construct a new PlaylistDialog object
   * @param dispatcher Event dispatcher
   * @param contains_audio_cb Callback function to check if given file contains audio stream
   * @param optional_path List files from custom path instead of the current one
   * @param stream_available_cb Callback function to check if songs can be added from URL (if not
   * informed, it is always possible)
   * @param fetch_playlist_cb Callback function to extract songs from a playlist URL (if not
   * informed, playlist URL cannot be imported)
   */
  PlaylistDialog(const std::shared_ptr<EventDispatcher>& dispatcher,
                 const std::function<bool(const util::File& file)>& contains_audio_cb,
                 const std::string& optional_path = "",
                 const std::function<bool()>& stream_available_cb = nullptr,
                 const PlaylistFetchCallback& fetch_playlist_cb = nullptr);

  /**
   * @brief Destroy PlaylistDialog object
   */
  ~PlaylistDialog() override;

  /**
   * @brief Set dialog as visible
   * @param operation Playlist operation (Create/Modify/Delete)
   */
  void Open(const model::PlaylistOperation& operation);

  /* ******************************************************************************************** */
  //! Custom implementation
 private:
  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element RenderImpl(const ftxui::Dimensions& curr_size) const override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEventImpl(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEventImpl(ftxui::Event event) override;

  /**
   * @brief Callback for when dialog is opened
   */
  void OnOpen() override;

  /**
   * @brief Callback for when dialog is closed
   */
  void OnClose() override;

 private:
  /**
   * @brief Create general buttons
   */
  void CreateButtons();

  /**
   * @brief Update UI state based on a few parameters (modified playlist, name, songs, ...)
   */
  void UpdateButtonState();

  /**
   * @brief Start editing playlist name
   */
  void StartRename();

  /**
   * @brief Handle event while playlist name is being edited
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnRenameEvent(const ftxui::Event& event);

  /**
   * @brief Stop editing playlist name
   * @param keep_name Flag to keep typed name (if valid), otherwise restore previous name
   */
  void FinishRename(bool keep_name);

  /**
   * @brief Render playlist name (with hint for rename keybinding) to display on pane border
   * @param max_columns Maximum columns available on pane border
   * @return Element Playlist name or text input (while editing)
   */
  ftxui::Element RenderPlaylistTitle(int max_columns) const;

  /**
   * @brief Get message displayed next to save button
   * @return Message (error, confirmation or reason why playlist cannot be saved yet) and its style
   */
  std::pair<std::string, ftxui::Decorator> GetSaveMessage() const;

  /**
   * @brief Show source of songs on the left pane and focus it
   * @param source Source to show (files or YouTube URL)
   */
  void ShowSource(Source source);

  /**
   * @brief Add song streamed from the given URL to modified playlist
   * @param url YouTube URL
   * @return Error message if URL was rejected, otherwise std::nullopt
   */
  std::optional<std::string> AddUrl(const std::string& url);

  //! Check if songs can be added from URL
  bool IsStreamAvailable() const { return !stream_available_cb_ || stream_available_cb_(); }

  /**
   * @brief Handle URL submitted by user: add a single song, or start importing songs from playlist
   * @param url YouTube URL
   * @return Result from URL (pending while playlist is being imported)
   */
  UrlInput::Result SubmitUrl(const std::string& url);

  /**
   * @brief Start importing songs from playlist URL in another thread (as it may take a while)
   * @param url YouTube playlist URL
   */
  void StartImport(const std::string& url);

  /**
   * @brief Add songs imported from playlist to modified playlist (if import has finished)
   */
  void FinishImport();

  /**
   * @brief Cancel playlist import (if running) and discard its result
   */
  void StopImport();

  /* ******************************************************************************************** */
  //! Variables

  std::filesystem::path base_path_;  //!< Default directory path to list files from in menu

  std::function<bool()> stream_available_cb_;  //!< Check if songs can be added from URL
  PlaylistFetchCallback fetch_playlist_cb_;    //!< Extract songs from playlist URL

  //! State for importing songs from playlist URL (running in another thread)
  struct Import {
    std::thread thread;                //!< Thread running extraction
    std::atomic<bool> cancel = false;  //!< Flag to cancel extraction
    std::mutex mutex;                  //!< Control access to result
    std::optional<std::pair<error::Code, std::vector<model::Song>>> result;  //!< Extracted songs
  };

  Import import_;  //!< Playlist import state

  //!< Operation to execute + playlist to be modified
  model::PlaylistOperation curr_operation_ =
      model::PlaylistOperation{.action = model::PlaylistOperation::Operation::None,
                               .playlist = model::Playlist{},
                               .other_names = {}};

  std::optional<model::Playlist> modified_playlist_;  //!< Playlist with latest modification

  FileMenu menu_files_;  //!< Menu containing all files from a given directory

  std::unique_ptr<UrlInput> url_input_;  //!< Text input to add songs from YouTube URL

  Source source_ = Source::Files;  //!< Source of songs currently shown on the left pane
  WindowButton btn_files_;         //!< Tab button to show files on the left pane
  WindowButton btn_youtube_;       //!< Tab button to show URL input on the left pane

  //! State for renaming playlist
  struct Rename {
    TextInput input;                   //!< Text input to type playlist name
    bool editing = false;              //!< Flag to indicate if playlist name is being edited
    std::optional<std::string> error;  //!< Reason why typed name was rejected
  };

  Rename rename_;           //!< Rename playlist state

  mutable ftxui::Box name_box_;  //!< Box to control if mouse cursor is over playlist name
  bool name_hovered_ = false;    //!< Flag to control if mouse cursor is over playlist name

  SongMenu menu_playlist_;  //!< Menu containing only files for the current playlist

  GenericButton btn_save_;  //!< Button to save (persist) playlist

  FlashMessage message_;  //!< Brief feedback shown below save button (e.g. after saving playlist)

  FocusController focus_ctl_;  //!< Controller to manage focus in registered elements
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_
