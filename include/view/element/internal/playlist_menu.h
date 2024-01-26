/**
 * \file
 * \brief  Class for rendering a customized menu containing playlist entries
 */

#ifndef INCLUDE_VIEW_ELEMENT_INTERNAL_PLAYLIST_MENU_H_
#define INCLUDE_VIEW_ELEMENT_INTERNAL_PLAYLIST_MENU_H_

#include <memory>
#include <string>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "model/playlist.h"
#include "view/element/internal/menu.h"
#include "view/element/text_animation.h"

#ifdef ENABLE_TESTS
namespace {
class SidebarTest;
}
#endif

namespace interface {

namespace internal {
class PlaylistMenu : public Menu<PlaylistMenu> {
  friend class Menu;

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

  //! A single menu entry (instead of using model::Playlist, wrap it with additional info)
  struct InternalPlaylist {
    model::Playlist playlist;
    bool collapsed;
  };

  //! Definition of menu content (list of menu entries)
  using InternalPlaylists = std::vector<InternalPlaylist>;

 public:
  //!< Callback definition for function that will be triggered when a menu entry is clicked/pressed
  using Callback = Callback<model::Playlist>;

  /**
   * @brief Construct a new FileMenu object
   * @param dispatcher Event dispatcher
   * @param force_refresh Callback function to update UI
   * @param on_click Callback function for click event on active entry
   */
  explicit PlaylistMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                        const TextAnimation::Callback& force_refresh, const Callback& on_click);

  /**
   * @brief Destroy Menu object
   */
  ~PlaylistMenu() override = default;

  /* ******************************************************************************************** */
  //! Mandatory API implementation
 private:
  //! Renders the element
  ftxui::Element RenderImpl();

  //! Handles an event (from mouse/keyboard)
  bool OnEventImpl(const ftxui::Event& event);

  //! Getter for menu entries size
  int GetSizeImpl() const;

  //! Getter for active entry as text (focused/selected)
  std::string GetActiveEntryAsTextImpl() const;

  //! Execute action on the active entry
  bool OnClickImpl();

  //! While on search mode, filter all entries to keep only those matching the given text
  void FilterEntriesBy(const std::string& text);

  /* ******************************************************************************************** */
  //! Setters and getters

  //! Save entries internally to render it as a menu
  void SetEntriesImpl(const model::Playlists& entries);

  //! Getter for entries
  InternalPlaylists GetEntriesImpl() const {
    return IsSearchEnabled() ? *filtered_entries_ : entries_;
  }

  //! Emplace a new entry
  void EmplaceImpl(const model::Playlist& entry) {
    LOG("Emplace a new entry to list");
    // TODO: evaluate this method... Maybe also use it on unit testing
    entries_.emplace_back(InternalPlaylist{.playlist = entry, .collapsed = false});
  }

  //! Set entry to be highlighted
  void SetEntryHighlightedImpl(const model::Song& entry);

  //! Reset highlighted entry
  void ResetHighlightImpl() { highlighted_.reset(); };

  //! Getter for active entry (focused/selected)
  std::optional<model::Playlist> GetActiveEntryImpl() const;

  //! Reset search mode (if enabled) and highlight the given entry
  void ResetSearchImpl() { filtered_entries_.reset(); }

  //! Create UI element for a single entry (playlist and song have different styles)
  ftxui::Element CreateEntry(int index, const std::string& text, bool is_highlighted,
                             bool is_playlist);

  //! Toggle collapse state for current selected entry (only available for playlist entries)
  bool ToggleActivePlaylist(bool collapse);

  /* ******************************************************************************************** */
  //! Variables
 private:
  InternalPlaylists entries_;  //!< List containing all parsed playlists

  //!< List containing only playlists (+ songs) matching the text from search
  std::optional<InternalPlaylists> filtered_entries_ = std::nullopt;

  //!< Index of song entry highlighted by owner
  std::optional<model::Song> highlighted_ = std::nullopt;

  Callback on_click_;  //!< Callback function to trigger when menu entry is clicked/pressed

  //!< Style for each element inside this component
  EntryStyles styles_ = EntryStyles{
      .prefix = ftxui::color(ftxui::Color::SteelBlue1Bis),
      .playlist =
          EntryStyles::State{
              .normal = Colored(ftxui::Color::DeepSkyBlue1, /*bold=*/true),
              .playing = Colored(ftxui::Color::PaleGreen1, /*bold=*/true),
          },
      .song =
          EntryStyles::State{
              .normal = Colored(ftxui::Color::White),
              .playing = Colored(ftxui::Color::SteelBlue1),
          },
  };

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
#endif
};

}  // namespace internal
}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_INTERNAL_PLAYLIST_MENU_H_
