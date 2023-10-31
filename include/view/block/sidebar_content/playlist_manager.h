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
#include "util/logger.h"
#include "view/element/button.h"
#include "view/element/style.h"
#include "view/element/tab.h"
#include "view/element/text_animation.h"

#ifdef ENABLE_TESTS
namespace {
class SidebarTest;
}
#endif

namespace interface {

/**
 * @brief Component to render and manage playlists
 */
class PlaylistManager : public TabItem {
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
  explicit PlaylistManager(const model::BlockIdentifier& id,
                           const std::shared_ptr<EventDispatcher>& dispatcher,
                           const FocusCallback& on_focus, const keybinding::Key& keybinding,
                           const std::shared_ptr<util::FileHandler>& file_handler, int max_columns);

  /**
   * @brief Destroy the PlaylistManager object
   */
  ~PlaylistManager() override = default;

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
      MenuEntryOption normal;
      MenuEntryOption playing;
    };

    State playlist;
    State song;
  };

  /* ******************************************************************************************** */
  //! Private methods

  //! Handle mouse wheel event
  bool OnMouseWheel(ftxui::Event event);

  //! Handle keyboard event mapped to a menu navigation command
  bool OnMenuNavigation(const ftxui::Event& event);

  /* ******************************************************************************************** */

  //! Getter for entries size
  int Size() const {
    int size = 0;
    for (const auto& entry : entries_) {
      // sum playlist + songs
      size += 1;
      if (entry.collapsed) size += entry.playlist.songs.size();
    }
    return size;
  }

  //! Getter for text entry at specified index
  std::string GetTextFromEntry(int index) {
    int tmp = 0;

    for (const auto& entry : entries_) {
      if (tmp == index) return entry.playlist.name;

      if (entry.collapsed) {
        for (int aux = 0; aux < entry.playlist.songs.size(); ++aux) {
          tmp++;  // count song
          if (tmp == index) return entry.playlist.songs.at(aux).filepath.filename().string();
        }
      }
      tmp++;  // count playlist
    }

    ERROR("Could not find the entry associated with the index=", index);
    return "";
  }

  //! Getter for selected index
  int* GetSelected() { return &selected_; }

  //! Getter for focused index
  int* GetFocused() { return &focused_; }

  //! Clamp both selected and focused indexes
  void Clamp();

  /* ******************************************************************************************** */
  //! Playlist operations

  /**
   * @brief Update content from active entry (decides if animation thread should run or not)
   */
  void UpdateActiveEntry();

  /**
   * @brief Execute click action on active entry (start playing selected playlist/song)
   * @return true if click action was executed, false otherwise
   */
  bool ClickOnActiveEntry();

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
  int selected_;                           //!< Index in list for selected entry
  int focused_;                            //!< Index in list for focused entry

  std::vector<ftxui::Box> boxes_;  //!< Single box for each entry in files list
  ftxui::Box box_;                 //!< Box for whole component

  TextAnimation animation_;  //!< Text animation for selected entry

  std::chrono::system_clock::time_point last_click_;  //!< Last timestamp that mouse was clicked

  GenericButton btn_create_;  //!< Button to create a new playlist
  GenericButton btn_modify_;  //!< Button to modify a playlist
  GenericButton btn_delete_;  //!< Button to delete a playlist

  //!< Style for each element inside this component
  EntryStyles styles_ = EntryStyles{
      .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
      .playlist =
          EntryStyles::State{
              .normal = Colored(ftxui::Color::White),
              .playing = Colored(ftxui::Color::SteelBlue1Bis),
          },
      .song =
          EntryStyles::State{
              .normal = Colored(ftxui::Color::White),
              .playing = Colored(ftxui::Color::SteelBlue1),
          },
  };

  //!< Style for a button displayed as tab button
  static Button::ButtonStyle kTabButtonStyle;

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
#endif
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BLOCK_SIDEBAR_CONTENT_PLAYLIST_MANAGER_H_
