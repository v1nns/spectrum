#include "view/base/keybinding.h"

namespace interface {

namespace keybinding {

/* ----------------------------------------- Navigation ----------------------------------------- */

Key Navigation::ArrowUp = Key::Special("\x1B[A");            //! ↑
Key Navigation::ArrowDown = Key::Event::Special("\x1B[B");   //! ↓
Key Navigation::ArrowLeft = Key::Event::Special("\x1B[D");   //! ←
Key Navigation::ArrowRight = Key::Event::Special("\x1B[C");  //! →

Key Navigation::Up = Key::Character('k');
Key Navigation::Down = Key::Character('j');
Key Navigation::Left = Key::Character('h');
Key Navigation::Right = Key::Character('l');

Key Navigation::Space = Key::Character(' ');
Key Navigation::Return = Key::Special({10});    //! Enter key
Key Navigation::Escape = Key::Special("\x1B");  //! Escape key

Key Navigation::Tab = Key::Event::Special({9});           //! Tab
Key Navigation::TabReverse = Key::Special({27, 91, 90});  //! Shift + Tab

Key Navigation::Home = Key::Special({27, 91, 72});           //! Home key
Key Navigation::End = Key::Special({27, 91, 70});            //! End key
Key Navigation::PageUp = Key::Special({27, 91, 53, 126});    //! PageUp key
Key Navigation::PageDown = Key::Special({27, 91, 54, 126});  //! PageDown key

Key Navigation::Backspace = Key::Special({127});              //! Backspace key
Key Navigation::CtrlBackspace = Key::Special({8});            //! Ctrl + Backspace
Key Navigation::CtrlBackspaceReverse = Key::Special("\027");  //! Ctrl + Backspace (alternative)

Key Navigation::Delete = Key::Special("\x1B[3~");  //! Delete key

Key Navigation::Close = Key::Character('q');

Key Navigation::EnableSearch = Key::Character('/');

/* ------------------------------------------ General ------------------------------------------- */

Key General::ExitApplication = Key::Character('q');
Key General::ShowTabHelper = Key::Special("\x1B[23~");  //! F11
Key General::ShowHelper = Key::Special("\x1B[24~");     //! F12

Key General::FocusSidebar = Key::Character('!');      //! Shift + 1
Key General::FocusInfo = Key::Character('@');         //! Shift + 2
Key General::FocusMainContent = Key::Character('#');  //! Shift + 3
Key General::FocusPlayer = Key::Character('$');       //! Shift + 4

/* ---------------------------------------- Main Content ---------------------------------------- */

Key MainContent::FocusVisualizer = Key::Character('1');
Key MainContent::FocusEqualizer = Key::Character('2');
Key MainContent::FocusLyric = Key::Character('3');

/* ------------------------------------------ Sidebar ------------------------------------------- */

Key Sidebar::FocusList = Key::Special("\x1BOP");      //! F1
Key Sidebar::FocusPlaylist = Key::Special("\x1BOQ");  //! F2

/* ---------------------------------------- Media Player ---------------------------------------- */

Key MediaPlayer::PlayOrPause = Key::Character('p');
Key MediaPlayer::Stop = Key::Character('s');

Key MediaPlayer::SkipToPrevious = Key::Character('<');
Key MediaPlayer::SkipToNext = Key::Character('>');

Key MediaPlayer::VolumeUp = Key::Character('+');
Key MediaPlayer::VolumeDown = Key::Character('-');
Key MediaPlayer::Mute = Key::Character('m');

Key MediaPlayer::SeekForward = Key::Character('f');
Key MediaPlayer::SeekBackward = Key::Character('b');

/* ----------------------------------------- Visualizer ----------------------------------------- */

Key Visualizer::ChangeAnimation = Key::Character('a');
Key Visualizer::ToggleFullscreen = Key::Character('h');
Key Visualizer::IncreaseBarWidth = Key::Character('.');
Key Visualizer::DecreaseBarWidth = Key::Character(',');

/* ----------------------------------------- Equalizer ------------------------------------------ */

Key Equalizer::ApplyFilters = Key::Character('a');
Key Equalizer::ResetFilters = Key::Character('r');

/* ------------------------------------------- Lyrics ------------------------------------------- */

// TODO: retry

/* ------------------------------------------- Files -------------------------------------------- */

// ...

/* ------------------------------------------ Playlist ------------------------------------------ */

Key Playlist::Create = Key::Character('c');
Key Playlist::Modify = Key::Character('o');
Key Playlist::Delete = Key::Character('d');

Key Playlist::Rename = Key::Character('r');
Key Playlist::Save = Key::Character('s');

}  // namespace keybinding

}  // namespace interface
