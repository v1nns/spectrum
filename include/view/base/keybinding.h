
/**
 * \file
 * \brief Table with all mapped keybindings to use within this application
 */

#ifndef INCLUDE_VIEW_BASE_KEYBINDING_H_
#define INCLUDE_VIEW_BASE_KEYBINDING_H_

#include "ftxui/component/event.hpp"

namespace interface {

namespace keybinding {

using Key = ftxui::Event;

//! Navigation keybindings
struct Navigation {
  static Key ArrowUp;
  static Key ArrowDown;
  static Key ArrowLeft;
  static Key ArrowRight;

  static Key Up;
  static Key Down;
  static Key Left;
  static Key Right;

  static Key Space;
  static Key Return;
  static Key Escape;

  static Key Tab;
  static Key TabReverse;

  static Key Home;
  static Key End;
  static Key PageUp;
  static Key PageDown;

  static Key Backspace;
  static Key CtrlBackspace;
  static Key CtrlBackspaceReverse;

  static Key Delete;

  static Key Close;

  static Key EnableSearch;
};

/* ********************************************************************************************** */

//! General keybindings
struct General {
  static Key ExitApplication;
  static Key ShowHelper;
  static Key ShowTabHelper;

  static Key FocusSidebar;
  static Key FocusInfo;
  static Key FocusMainContent;
  static Key FocusPlayer;
};

/* ********************************************************************************************** */

//! Main Tab keybindings
struct MainContent {
  static Key FocusVisualizer;
  static Key FocusEqualizer;
  static Key FocusLyric;
};

/* ********************************************************************************************** */

//! Sidebar keybindings
struct Sidebar {
  static Key FocusList;
  static Key FocusPlaylist;
};

/* ********************************************************************************************** */

//! Media player keybindings
struct MediaPlayer {
  static Key PlayOrPause;
  static Key Stop;
  static Key ClearSong;

  static Key SkipToPrevious;
  static Key SkipToNext;

  static Key VolumeUp;
  static Key VolumeDown;
  static Key Mute;

  static Key SeekForward;
  static Key SeekBackward;
};

/* ********************************************************************************************** */

//! Visualizer keybindings
struct Visualizer {
  static Key ChangeAnimation;
  static Key ToggleFullscreen;
  static Key IncreaseBarWidth;
  static Key DecreaseBarWidth;
};

/* ********************************************************************************************** */

//! Equalizer keybindings
struct Equalizer {
  static Key ApplyFilters;
  static Key ResetFilters;

  static Key IncreaseBarWidth;
  static Key DecreaseBarWidth;
};

/* ********************************************************************************************** */

//! Lyric keybindings
struct Lyric {
  // TODO: retry
};

/* ********************************************************************************************** */

//! Files keybindings
struct Files {};

/* ********************************************************************************************** */

//! Playlist keybindings
struct Playlist {
  static Key Create;
  static Key Modify;
  static Key Delete;
};

}  // namespace keybinding
}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_KEYBINDING_H_
