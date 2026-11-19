/**
 * \file
 * \brief  Class for rendering a menu with results from searching an artist on YouTube
 */

#ifndef INCLUDE_VIEW_ELEMENT_INTERNAL_YOUTUBE_MENU_H_
#define INCLUDE_VIEW_ELEMENT_INTERNAL_YOUTUBE_MENU_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "view/element/internal/base_menu.h"
#include "view/element/style.h"
#include "view/element/text_animation.h"

namespace interface::internal {

/**
 * @brief Menu with artists or channels found for an artist, or with releases (each one followed by
 * its tracks, when expanded)
 */
class YoutubeMenu : public BaseMenu<YoutubeMenu> {
  friend class BaseMenu;

  //! Put together all possible styles for an entry in this component
  struct Style {
    ftxui::Decorator mark;
    ftxui::Decorator missing;
    ftxui::Decorator detail;
    ftxui::Decorator fallback;
    MenuEntryOption entry;
  };

  //! Columns used by marker before release, telling whether it is expanded (e.g. "▸ ")
  static constexpr int kArrowColumns = 2;

  //! Columns used by indentation before track (as it belongs to the release above it)
  static constexpr int kIndentColumns = 2;

  //! Text is never animated (it is cut with an ellipsis instead)
  static constexpr int GetMaxColumnsForIconImpl() { return -1; }

 public:
  //! Single entry in menu
  struct Row {
    //! Possible types of entry
    enum class Kind : std::uint8_t {
      Artist,           //!< Artist found on MusicBrainz
      Channel,          //!< Channel found for artist on YouTube
      SearchChannels,   //!< Search channels on YouTube instead of artists on MusicBrainz
      SearchPlaylists,  //!< Search playlists on YouTube instead
      Group,            //!< Group of releases (e.g. albums), which may be expanded to show them
      Release,          //!< Release (or playlist), which may be expanded to show its tracks
      Track,            //!< Track from release
    };

    Kind kind;           //!< Entry type
    int index = 0;       //!< Index of artist, channel, group or release
    int track = 0;       //!< Index of track inside release (only for tracks)
    std::string text;    //!< Main text (cut with an ellipsis when there is not enough space)
    std::string detail;  //!< Text aligned to the right (e.g. track duration)
    int depth = 0;       //!< Indentation level (e.g. release inside a group)
    bool leaf = false;   //!< Release that is never expanded (e.g. a single), so it has no arrow
  };

  //! How many songs from a release (or track) are in the playlist being edited
  enum class Mark : std::uint8_t {
    None,     //!< None of them
    Some,     //!< Some of them
    All,      //!< All of them (that can be added)
    Missing,  //!< Track was not found on YouTube, so it cannot be added
  };

  //! State from entry that may change at any time (e.g. songs removed from playlist)
  struct RowState {
    std::optional<Mark> mark;           //!< Songs in playlist (nothing for entries without songs)
    bool expanded = false;              //!< Release shows its tracks below it
    std::optional<std::string> detail;  //!< Replace text aligned to the right (e.g. a spinner)
  };

  //!< Callback definition for function that will be triggered when a menu entry is clicked/pressed
  using Callback = Callback<Row>;

  //! Callback to get current state from entry
  using StateCallback = std::function<RowState(const Row& row)>;

  /**
   * @brief Construct a new YoutubeMenu object
   * @param dispatcher Event dispatcher
   * @param force_refresh Callback function to update UI
   * @param on_click Callback function for click event on active entry
   * @param get_state Callback function to get current state from entry
   */
  explicit YoutubeMenu(const std::shared_ptr<EventDispatcher>& dispatcher,
                       const TextAnimation::Callback& force_refresh, const Callback& on_click,
                       const StateCallback& get_state);

  /**
   * @brief Destroy Menu object
   */
  ~YoutubeMenu() override = default;

  /* ******************************************************************************************** */
  //! Mandatory API implementation
 private:
  //! Renders the element
  ftxui::Element RenderImpl();

  //! Handles an event (from mouse/keyboard)
  bool OnEventImpl(const ftxui::Event&) { return false; }

  //! Getter for menu entries size
  int GetSizeImpl() const { return static_cast<int>(entries_.size()); }

  //! Getter for active entry as text (empty, so it is never animated)
  std::string GetActiveEntryAsTextImpl() const { return ""; }

  //! Execute action on the active entry
  bool OnClickImpl();

  //! Search mode is never enabled
  void FilterEntriesBy(const std::string&) {}

  //! Search mode is never enabled
  void ResetSearchImpl() {}

  /* ******************************************************************************************** */
  //! Setters and getters

  //! Save entries internally to render it as a menu
  void SetEntriesImpl(const std::vector<Row>& entries) { entries_ = entries; }

  //! Getter for entries
  std::vector<Row> GetEntriesImpl() const { return entries_; }

  //! Getter for active entry (focused/selected)
  std::optional<Row> GetActiveEntryImpl() const;

  //! Update style for menu entries with colors from current theme (called before rendering)
  void UpdateStyleImpl();

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::vector<Row> entries_;  //!< List containing all entries

  Callback on_click_;        //!< Callback function to trigger when menu entry is clicked/pressed
  StateCallback get_state_;  //!< Callback function to get current state from entry

  Style style_;  //!< Style for each element inside this component (updated on render)
};

}  // namespace interface::internal
#endif  // INCLUDE_VIEW_ELEMENT_INTERNAL_YOUTUBE_MENU_H_
