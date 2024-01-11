/**
 * \file
 * \brief  Class for tab view containing playlist manager
 */

#ifndef INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_PLAYLIST_MANAGER_H_
#define INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_PLAYLIST_MANAGER_H_

#include <optional>
#include <string_view>

#include "model/playlist.h"
#include "util/file_handler.h"
#include "view/element/button.h"
#include "view/element/tab.h"

#ifdef ENABLE_TESTS
namespace {
class SidebarTest;
}
#endif

namespace interface {

/**
 * @brief Component to render and manage playlists
 */
class PlaylistViewer : public TabItem {
  static constexpr std::string_view kTabName = "playlist";  //!< Tab title
  static constexpr int kMaxIconColumns = 2;                 //!< Maximum columns for Icon

 public:
  /**
   * @brief Construct a new PlaylistManager object
   * @param id Parent block identifier
   * @param dispatcher Block event dispatcher
   * @param on_focus Callback function to ask for focus
   * @param keybinding Keybinding to set item as active
   * @param file_handler Utility handler to manage any file operation
   * @param max_columns Maximum number of visual columns to be used by this element
   */
  explicit PlaylistViewer(const model::BlockIdentifier& id,
                          const std::shared_ptr<EventDispatcher>& dispatcher,
                          const FocusCallback& on_focus, const keybinding::Key& keybinding,
                          const std::shared_ptr<util::FileHandler>& file_handler, int max_columns);

  /**
   * @brief Destroy the PlaylistManager object
   */
  ~PlaylistViewer() override = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEvent(ftxui::Event& event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /* ******************************************************************************************** */
  //! Custom class to link playlists with UI state
 private:
  struct InternalPlaylist {
    model::Playlist playlist;
    bool collapsed;
  };

  //! Put together all possible styles for an entry in this component
  struct EntryStyles {
    ftxui::Decorator prefix;

    struct State {
      // MenuEntryOption normal;
      // MenuEntryOption playing;
    };

    State playlist;
    State song;
  };

  /* ******************************************************************************************** */
  //! UI initialization

  /**
   * @brief Initialize all UI buttons to manage playlists
   */
  void CreateButtons();

  /* ******************************************************************************************** */
  //! Variables
 private:
  int max_columns_;  //!< Maximum number of columns (characters in a single line) available to use
  std::shared_ptr<util::FileHandler> file_handler_;  //!< Utility class to manage files (read/write)

  std::optional<model::Song> curr_playing_ = std::nullopt;  //!< Current song playing

  std::vector<InternalPlaylist> entries_;  //!< List containing all playlists created by user
  ftxui::Box box_;                         //!< Box for whole component

  std::chrono::system_clock::time_point last_click_;  //!< Last timestamp that mouse was clicked

  GenericButton btn_create_;  //!< Button to create a new playlist
  GenericButton btn_modify_;  //!< Button to modify a playlist
  GenericButton btn_delete_;  //!< Button to delete a playlist

  //!< Style for each element inside this component
  EntryStyles styles_ = EntryStyles{
      .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
      // .playlist =
      // EntryStyles::State{
      // .normal = Colored(ftxui::Color::White),
      // .playing = Colored(ftxui::Color::SteelBlue1Bis),
      // },
      // .song =
      // EntryStyles::State{
      // .normal = Colored(ftxui::Color::White),
      // .playing = Colored(ftxui::Color::SteelBlue1),
      // },
  };

  //!< Style for a button displayed as tab button
  static Button::Style kTabButtonStyle;

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_PLAYLIST_MANAGER_H_
