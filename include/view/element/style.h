/**
 * \file
 * \brief Header for UI style (colors and size constants shared by blocks)
 */

#ifndef INCLUDE_VIEW_ELEMENT_STYLE_H_
#define INCLUDE_VIEW_ELEMENT_STYLE_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "ftxui/dom/canvas.hpp"
#include "ftxui/screen/color.hpp"

namespace interface {

//! Width for blocks placed in the left column (Sidebar and FileInfo), which must be the same
static constexpr int kLeftColumnWidth = 36;

/**
 * @brief All colors used by UI, grouped by the element using them (filled by each theme available,
 * check GetThemes)
 */
struct Theme {
  using Color = ftxui::Color;

  //! Colors for a single state from an element (default color means "do not change it")
  struct State {
    Color foreground{};  //!< Color for foreground
    Color background{};  //!< Color for background
    Color border{};      //!< Color for border
  };

  //! Colors for all states from a button
  struct ButtonStates {
    State normal{};     //!< Colors for normal state
    State focused{};    //!< Colors for focused state
    State selected{};   //!< Colors for selected state
    State pressed{};    //!< Colors for pressed state
    State disabled{};   //!< Colors for disabled state
    State highlight{};  //!< Colors for highlighted state (e.g. a single letter)
  };

  //! Color stop from a gradient (position goes from 0.0 to 1.0)
  struct ColorStop {
    float position;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  };

  /* ------------------------------------------- Screen ----------------------------------------- */

  //! Whole screen (default color means "use the one from terminal")
  struct Screen {
    Color foreground;  //!< Anything without a color of its own (e.g. border from unfocused block)
    Color background;
  };

  Screen screen;  //!< Colors for whole screen

  /* ------------------------------------------- Block ------------------------------------------ */

  //! Title and border from any block
  struct Block {
    State title;
    State title_focused;

    Color border;
    Color border_focused;

    //! Tab buttons on block border
    ButtonStates tab;

    //! Any other button on block border (e.g. help and exit)
    ButtonStates window_button;
  };

  Block block;  //!< Colors for any block

  /* ------------------------------------------- Menu ------------------------------------------- */

  //! Menus listing files, playlists and songs
  struct Menu {
    Color prefix;          //!< Icon before selected entry
    Color prefix_playing;  //!< Icon before entry that is playing
    State cursor;          //!< Selected (or hovered) entry, no matter its type
    Color title;           //!< Menu title (e.g. current directory)
    Color search;          //!< Label and text typed in search input

    Color directory;
    Color file;
    Color file_playing;

    Color playlist;
    Color playlist_playing;
    Color song;
    Color song_playing;
  };

  Menu menu;  //!< Colors for menus placed in a block

  /* ------------------------------------------ Sidebar ----------------------------------------- */

  //! Sidebar block
  struct Sidebar {
    //! Buttons to manage playlists
    ButtonStates button;
  };

  Sidebar sidebar;  //!< Colors for sidebar block

  /* ---------------------------------------- Information --------------------------------------- */

  //! Block with song information
  struct FileInfo {
    Color title;        //!< Song title, shown on the first line
    Color artist;       //!< Song artist, shown right below title
    Color field;        //!< Name of each detail from song (e.g. format)
    Color value;        //!< Value of each detail from song
    Color value_empty;  //!< Message shown when there is no song
  };

  FileInfo file_info;  //!< Colors for block with song information

  /* ---------------------------------------- Main content -------------------------------------- */

  //! Spectrum visualizer
  struct Visualizer {
    Color text;  //!< Brief message and selected entry from animation picker

    //! Gradient from the lowest to the highest part of spectrum
    std::array<ColorStop, 4> gradient{};

    //! Single color to use instead of gradient (for themes limited to colors from terminal)
    std::optional<Color> solid;
  };

  Visualizer visualizer;  //!< Colors for spectrum visualizer

  //! Audio equalizer
  struct Equalizer {
    Color text;  //!< Frequency, gain and preset picker

    //! Frequency bar (border is not used)
    State bar;
    State bar_hovered;
    State bar_focused;

    //! Buttons to apply and reset filters
    ButtonStates button;
  };

  Equalizer equalizer;  //!< Colors for audio equalizer

  //! Song lyric
  struct Lyric {
    Color text;
  };

  Lyric lyric;  //!< Colors for song lyric

  /* ------------------------------------------- Player ----------------------------------------- */

  //! Media player block
  struct Player {
    Color text;  //!< Volume, song position, repeat and shuffle modes
    Color volume_muted;
    Color warning;

    Color play;  //!< Icon from button to play or pause
    Color stop;  //!< Icon from button to stop
    Color skip;  //!< Icon from buttons to skip song

    Color button_border;
    Color button_border_focused;

    //! Bar with song position (border is not used)
    State duration;
    State duration_focused;
  };

  Player player;  //!< Colors for media player block

  /* ------------------------------------------- Dialog ----------------------------------------- */

  //! Any dialog, and all elements placed inside it
  struct Dialog {
    Color border;
    Color background;
    Color background_error;  //!< Background for error dialog
    Color foreground;
    Color foreground_error;  //!< Foreground for error dialog

    Color text;        //!< Title and content
    Color label;       //!< Label for input
    Color hint;        //!< Hint for the next possible action
    Color keybinding;  //!< Keys listed in help
    Color success;     //!< Feedback when action was accepted
    Color error;       //!< Feedback when action was rejected

    Color pane_border;  //!< Border from pane inside dialog
    Color pane_border_focused;
    Color pane_title;  //!< Title on pane border

    //! Text input
    State input;
    Color input_placeholder;

    //! Menus listing files and songs
    Color menu_directory;
    Color menu_file;
    Color menu_file_playing;
    Color menu_song;
    Color menu_tag;  //!< Tag before song (e.g. streamed from URL)

    //! Main button (e.g. save)
    ButtonStates button;

    //! Tab buttons on pane border
    ButtonStates tab;

    //! Buttons to answer a question
    ButtonStates answer;
  };

  Dialog dialog;  //!< Colors for dialogs

  /* ------------------------------------------- Picker ----------------------------------------- */

  //! Theme picker (shown over all blocks)
  struct Picker {
    Color border;
    Color entry;
    Color entry_selected;
  };

  Picker picker;  //!< Colors for theme picker
};

/**
 * @brief Theme that may be chosen by user
 */
struct ThemeOption {
  std::string_view id;    //!< Identifier saved in settings
  std::string_view name;  //!< Name shown by UI
  Theme colors;           //!< All colors from theme
};

/**
 * @brief Get all themes available to choose from (the first one is the default theme)
 * @return List of themes
 */
const std::vector<ThemeOption>& GetThemes();

namespace internal {

//! Storage for theme currently in use (do not use it directly, check functions below)
inline Theme& CurrentTheme() {
  static Theme theme = GetThemes().front().colors;
  return theme;
}

}  // namespace internal

/**
 * @brief Get theme with all colors to use in UI
 * @note Colors must be read when rendering (and not kept by elements), as theme may be replaced
 * @return Theme
 */
inline const Theme& GetTheme() { return internal::CurrentTheme(); }

/**
 * @brief Replace theme used by UI, new colors are applied on the next render
 * @note Must be called only from UI thread (the same one that renders), as there is no locking
 * @param theme New theme
 */
inline void SetTheme(const Theme& theme) { internal::CurrentTheme() = theme; }

/**
 * @brief Fill canvas with background color from theme (as canvas replaces anything drawn behind it,
 * including its background). Nothing is done when theme uses the background from terminal
 * @param canvas Canvas to fill
 */
inline void FillBackground(ftxui::Canvas& canvas) {
  static constexpr int kCellWidth = 2;   //!< Canvas points in a single cell (horizontally)
  static constexpr int kCellHeight = 4;  //!< Canvas points in a single cell (vertically)

  const ftxui::Color& background = GetTheme().screen.background;
  if (background == ftxui::Color{}) return;

  for (int y = 0; y < canvas.height(); y += kCellHeight) {
    for (int x = 0; x < canvas.width(); x += kCellWidth) {
      canvas.Style(x, y,
                   [&background](ftxui::Pixel& pixel) { pixel.background_color = background; });
    }
  }
}

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_STYLE_H_
