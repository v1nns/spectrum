/**
 * \file
 * \brief  Class for dialog to manage a playlist
 */

#ifndef INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_
#define INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_

#include "model/playlist_operation.h"
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
  ftxui::Element RenderImpl() const override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEventImpl(const ftxui::Event& event) override;

  //! Create general buttons
  void CreateButtons();

  /* ******************************************************************************************** */
  //! Variables

  util::FileHandler file_handler_;  //!< Utility class to manage files (read/write)

  //!< Operation to execute + playlist to be modified
  model::PlaylistOperation curr_operation_ = model::PlaylistOperation{
      .action = model::PlaylistOperation::Operation::None, .playlist = model::Playlist{}};

  GenericButton btn_add_;     //!< Button to add new song to playlist
  GenericButton btn_remove_;  //!< Button to remove selected song to playlist
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_PLAYLIST_DIALOG_H_
