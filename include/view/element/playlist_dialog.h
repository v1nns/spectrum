/**
 * \file
 * \brief  Class for dialog to manage a playlist
 */

#ifndef INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_

#include <ftxui/component/component_base.hpp>
#include <optional>
#include <string_view>

#include "ftxui/component/screen_interactive.hpp"
#include "model/playlist_operation.h"
#include "view/base/dialog.h"
#include "view/base/event_dispatcher.h"
#include "view/element/button.h"
#include "view/element/focus_controller.h"
#include "view/element/menu.h"

namespace interface {

/**
 * @brief Customized dialog box to manage a single playlist
 */
class PlaylistDialog : public Dialog {
  static constexpr int kMinColumns = 45;  //!< Minimum columns for Element
  static constexpr int kMinLines = 25;    //!< Minimum lines for Element

 public:
  /**
   * @brief Construct a new PlaylistDialog object
   * @param dispatcher Event dispatcher
   * @param optional_path List files from custom path instead of the current one
   */
  PlaylistDialog(const std::shared_ptr<EventDispatcher>& dispatcher,
                 const std::string& optional_path = "");

  /**
   * @brief Destroy PlaylistDialog object
   */
  virtual ~PlaylistDialog() = default;

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

  /* ******************************************************************************************** */
  //! Variables

  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks

  static std::filesystem::path base_path_;  //!< Default directory path to list files from in menu

  //!< Operation to execute + playlist to be modified
  model::PlaylistOperation curr_operation_ = model::PlaylistOperation{
      .action = model::PlaylistOperation::Operation::None, .playlist = model::Playlist{}};

  std::optional<model::Playlist> modified_playlist_;  //!< Playlist with latest modification

  FileMenu menu_files_;  //!< Menu containing all files from a given directory

  // TODO:
  // - encapsulate in a struct
  // - create <C-Backspace> to delete word
  // - do not use ftxui::Input (maybe, to avoid the cursor bug)

  // TODO: DOC
  struct Input {
    //!< Default text to show when name is empty
    static constexpr std::string_view kDefaultName = "<unnamed>";

    std::string name;         //!< Playlist name to display on text/input
    int cursor_position = 0;  //!< Cursor position in text input
    bool edit_mode = false;   //!< Flag to control edit mode, when enabled, text input is displayed

    ftxui::Element Render(int min) const {
      // TODO: must show unnamed when is on edit mode and input name is empty
      if (name.empty() && !edit_mode) {
        return ftxui::text(std::string(kDefaultName));
      }

      auto decorator =
          edit_mode ? ftxui::bgcolor(ftxui::Color::Grey11) : ftxui::color(ftxui::Color::Grey11);

      auto size = ftxui::size(ftxui::WIDTH, ftxui::EQUAL, std::min(min, (int)name.length()));

      return ftxui::text(name) | decorator | size;
    }

    bool OnEvent(const ftxui::Event& event) {
      using Keybind = keybinding::Navigation;

      bool event_handled = false;

      // Any alphabetic character
      if (event.is_character()) {
        name.insert(cursor_position, event.character());
        cursor_position++;
        event_handled = true;
      }

      // Backspace
      if (event == Keybind::Backspace && !name.empty()) {
        if (cursor_position > 0) {
          name.erase(cursor_position - 1, 1);
          cursor_position--;
        }

        event_handled = true;
      }

      // Delete
      if (event == Keybind::Delete && !name.empty() && cursor_position < name.size()) {
        name.erase(cursor_position, 1);
        if (cursor_position > 0) cursor_position--;

        event_handled = true;
      }

      // Arrow left
      if (event == Keybind::ArrowLeft) {
        if (cursor_position > 0) cursor_position--;
        event_handled = true;
      }

      // Arrow right
      if (event == Keybind::ArrowRight) {
        if (auto size = (int)name.size(); cursor_position < size) cursor_position++;
        event_handled = true;
      }

      // Exit from edit mode
      if (edit_mode && (event == Keybind::Return || event == Keybind::Escape)) {
        LOG("Exiting from edit mode");
        edit_mode = false;
        event_handled = true;
      }

      return event_handled;
    }

    bool IsEditing() const { return edit_mode; }
    void EnableEdit() { edit_mode = true; }
    void DisableEdit() { edit_mode = false; }

    void Clear() {
      name.clear();
      cursor_position = 0;
      edit_mode = false;
    }
  };

  Input input_playlist_;    // Text input to display playlistn name
  SongMenu menu_playlist_;  //!< Menu containing only files for the current playlist

  GenericButton btn_save_;  //!< Button to save (persist) playlist

  FocusController focus_ctl_;  //!< Controller to manage focus in registered elements
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_
