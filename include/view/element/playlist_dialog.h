/**
 * \file
 * \brief  Class for dialog to manage a playlist
 */

#ifndef INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_

#include <optional>

#include "model/playlist.h"
#include "util/file_handler.h"
#include "view/base/dialog.h"
#include "view/element/button.h"

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
   */
  PlaylistDialog();

  /**
   * @brief Destroy PlaylistDialog object
   */
  virtual ~PlaylistDialog() = default;

  /* ******************************************************************************************** */
  //! Custom implementation
 private:
  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element RenderImpl() const override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEventImpl(const ftxui::Event& event) override;

  /**
   * @brief Callback to notify when dialog is opened
   */
  void OnOpen() override;

  /**
   * @brief Set playlist to be modified
   * @param playlist Playlist object
   */
  void SetPlaylist(const model::Playlist playlist) { playlist_ = playlist; }

  //! Create general buttons
  void CreateButtons();

  /* ******************************************************************************************** */
  //! Variables

  util::FileHandler file_handler_;  //!< Utility class to manage files (read/write)
  util::Files entries_;             //!< List containing files from parsed directory

  GenericButton btn_add_;                    //!< Button to add new song to playlist
  GenericButton btn_remove_;                 //!< Button to remove selected song to playlist
  std::optional<model::Playlist> playlist_;  //!< Playlist to be modified
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_
