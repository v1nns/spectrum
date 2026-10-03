/**
 * \file
 * \brief Header for UI style (colors and size constants shared by blocks)
 */

#ifndef INCLUDE_VIEW_ELEMENT_STYLE_H_
#define INCLUDE_VIEW_ELEMENT_STYLE_H_

#include <array>
#include <cstdint>

#include "ftxui/screen/color.hpp"

namespace interface {

//! Width for blocks placed in the left column (Sidebar and FileInfo), which must be the same
static constexpr int kLeftColumnWidth = 36;

/**
 * @brief All colors used by UI, grouped by the element using them
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

  /* ------------------------------------------- Block ------------------------------------------ */

  //! Title and border from any block
  struct Block {
    State title = State{.foreground = Color::GrayLight, .background = Color::GrayDark};
    State title_focused =
        State{.foreground = Color::LightSteelBlue1, .background = Color::SteelBlue3};

    Color border_focused = Color::SteelBlue3;

    //! Tab buttons on block border
    ButtonStates tab = ButtonStates{
        .normal = State{.foreground = Color::GrayDark},
        .focused = State{.foreground = Color::GrayLight, .background = Color::GrayDark},
        .selected = State{.foreground = Color::LightSteelBlue1, .background = Color::SteelBlue3},
    };

    //! Any other button on block border (e.g. help and exit)
    ButtonStates window_button = ButtonStates{
        .focused = State{.foreground = Color::GrayLight, .background = Color::GrayDark},
        .pressed = State{.foreground = Color::GrayLight, .background = Color::GrayDark},
    };
  };

  Block block;  //!< Colors for any block

  /* ------------------------------------------- Menu ------------------------------------------- */

  //! Menus listing files, playlists and songs
  struct Menu {
    Color prefix = Color::SteelBlue1Bis;  //!< Icon before entry
    Color title = Color::White;           //!< Menu title (e.g. current directory)
    Color search = Color::White;          //!< Label for search input

    Color directory = Color::Green;
    Color file = Color::White;
    Color file_playing = Color::SteelBlue1;

    Color playlist = Color::SteelBlue1;
    Color playlist_playing = Color::PaleGreen1;
    Color song = Color::White;
    Color song_playing = Color::SteelBlue1Bis;
  };

  Menu menu;  //!< Colors for menus placed in a block

  /* ------------------------------------------ Sidebar ----------------------------------------- */

  //! Sidebar block
  struct Sidebar {
    //! Buttons to manage playlists
    ButtonStates button = ButtonStates{
        .normal = State{.foreground = Color::Grey11, .background = Color::SteelBlue1},
        .focused = State{.foreground = Color::DeepSkyBlue4Ter, .background = Color::LightSkyBlue1},
        .pressed = State{.foreground = Color::SkyBlue1, .background = Color::Blue1},
        .disabled = State{.foreground = Color::Grey35, .background = Color::SteelBlue},
        .highlight = State{.foreground = Color::DeepPink4Bis},
    };
  };

  Sidebar sidebar;  //!< Colors for sidebar block

  /* ---------------------------------------- Information --------------------------------------- */

  //! Block with song information
  struct FileInfo {
    Color field = Color::SteelBlue1;
    Color value = Color::LightSteelBlue1;
    Color value_empty = Color::LightSteelBlue3;  //!< Value when there is no song
  };

  FileInfo file_info;  //!< Colors for block with song information

  /* ---------------------------------------- Main content -------------------------------------- */

  //! Spectrum visualizer
  struct Visualizer {
    Color text = Color::White;  //!< Brief message and selected entry from animation picker

    //! Gradient from the lowest to the highest part of spectrum
    std::array<ColorStop, 4> gradient{{
        {0.0F, 95, 135, 215},
        {0.3F, 115, 155, 215},
        {0.6F, 155, 188, 235},
        {0.8F, 185, 208, 252},
    }};
  };

  Visualizer visualizer;  //!< Colors for spectrum visualizer

  //! Audio equalizer
  struct Equalizer {
    Color text = Color::White;  //!< Frequency, gain and preset picker

    //! Frequency bar (border is not used)
    State bar = State{.foreground = Color::SteelBlue3, .background = Color::LightSteelBlue3};
    State bar_hovered =
        State{.foreground = Color::SlateBlue1, .background = Color::LightSteelBlue1};
    State bar_focused = State{.foreground = Color::RedLight, .background = Color::LightSteelBlue1};

    //! Buttons to apply and reset filters
    ButtonStates button = ButtonStates{
        .normal = State{.foreground = Color::White, .border = Color::White},
        .focused = State{.border = Color::SteelBlue3},
        .pressed = State{.foreground = Color::SteelBlue3,
                         .background = Color::LightSteelBlue3,
                         .border = Color::SteelBlue3},
        .disabled = State{.foreground = Color::GrayDark, .border = Color::GrayDark},
        .highlight = State{.foreground = Color::IndianRed},
    };
  };

  Equalizer equalizer;  //!< Colors for audio equalizer

  //! Song lyric
  struct Lyric {
    Color text = Color::White;
  };

  Lyric lyric;  //!< Colors for song lyric

  /* ------------------------------------------- Player ----------------------------------------- */

  //! Media player block
  struct Player {
    Color text = Color::White;  //!< Volume, song position, repeat and shuffle modes
    Color volume_muted = Color::Red3Bis;
    Color warning = Color::Yellow;

    Color play = Color::SpringGreen2;  //!< Icon from button to play or pause
    Color stop = Color::Red;           //!< Icon from button to stop
    Color skip = Color::SteelBlue;     //!< Icon from buttons to skip song

    Color button_border = Color::GrayDark;
    Color button_border_focused = Color::SteelBlue3;

    //! Bar with song position (border is not used)
    State duration = State{.foreground = Color::SteelBlue3, .background = Color::LightSteelBlue3};
    State duration_focused =
        State{.foreground = Color::SlateBlue1, .background = Color::LightSteelBlue1};
  };

  Player player;  //!< Colors for media player block

  /* ------------------------------------------- Dialog ----------------------------------------- */

  //! Any dialog, and all elements placed inside it
  struct Dialog {
    Color border = Color::Grey85;
    Color background = Color::SteelBlue;
    Color background_error = Color::DarkRedBis;  //!< Background for error dialog
    Color foreground = Color::Grey93;

    Color text = Color::Black;                 //!< Title and content
    Color label = Color::Grey93;               //!< Label for input
    Color hint = Color::Grey82;                //!< Hint for the next possible action
    Color keybinding = Color::PaleTurquoise1;  //!< Keys listed in help
    Color success = Color::DarkSeaGreen2Bis;   //!< Feedback when action was accepted
    Color error = Color::MistyRose1;           //!< Feedback when action was rejected

    Color pane_border = Color::Grey11;  //!< Border from pane inside dialog
    Color pane_border_focused = Color::LightSkyBlue1;
    Color pane_title = Color::Grey11;  //!< Title on pane border

    //! Text input
    State input = State{.foreground = Color::Grey93, .background = Color::Grey11};
    Color input_placeholder = Color::Grey50;

    //! Menus listing files and songs
    Color menu_directory = Color::DarkSeaGreen2Bis;
    Color menu_file = Color::Grey11;
    Color menu_file_playing = Color::SteelBlue1;
    Color menu_song = Color::Grey11;
    Color menu_tag = Color::LightPink1;  //!< Tag before song (e.g. streamed from URL)

    //! Main button (e.g. save)
    ButtonStates button = ButtonStates{
        .normal = State{.foreground = Color::Black,
                        .background = Color::SkyBlue3,
                        .border = Color::GrayDark},
        .focused = State{.foreground = Color::LightSkyBlue1,
                         .background = Color::DeepSkyBlue4Ter,
                         .border = Color::LightSkyBlue1},
        .pressed = State{.foreground = Color::SteelBlue3,
                         .background = Color::LightSteelBlue3,
                         .border = Color::SteelBlue3},
        .disabled = State{.foreground = Color::Grey35,
                          .background = Color::SteelBlue,
                          .border = Color::GrayDark},
    };

    //! Tab buttons on pane border
    ButtonStates tab = ButtonStates{
        .normal = State{.foreground = Color::Grey11, .background = Color::SteelBlue},
        .focused = State{.foreground = Color::Grey11, .background = Color::LightSkyBlue1},
        .selected = State{.foreground = Color::Grey11, .background = Color::LightSkyBlue1},
    };

    //! Buttons to answer a question
    ButtonStates answer = ButtonStates{
        .normal = State{.foreground = Color::Black, .background = Color::SteelBlue1},
        .focused = State{.foreground = Color::Black, .background = Color::LightSkyBlue1},
        .selected = State{.foreground = Color::Black, .background = Color::Grey93},
        .pressed = State{.foreground = Color::SkyBlue1, .background = Color::Blue1},
        .highlight = State{.foreground = Color::DarkRed},
    };
  };

  Dialog dialog;  //!< Colors for dialogs
};

/**
 * @brief Get theme with all colors to use in UI
 * @return Theme
 */
inline const Theme& GetTheme() {
  static const Theme theme;
  return theme;
}

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_STYLE_H_
