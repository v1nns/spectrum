#include "view/element/help_dialog.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/terminal.hpp"
#include "util/formatter.h"
#include "view/base/dialog.h"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

namespace {

//! Get user-friendly text for a keybinding
std::string ToString(const keybinding::Key& key) { return util::EventToString(key); }

//! Join keybindings using a separator (e.g. "+/-")
std::string Join(const keybinding::Key& first, const keybinding::Key& second) {
  return ToString(first) + "/" + ToString(second);
}

}  // namespace

/* ********************************************************************************************** */

HelpDialog::HelpDialog(const std::shared_ptr<EventDispatcher>& dispatcher)
    : Dialog(dispatcher,
             Size{.height = kHeightRatio,
                  .min_column = kMaxColumns,
                  .min_line = kMinLines,
                  .max_line = kMaxLines},
             Style{.background = &Theme::Dialog::background,
                   .foreground = &Theme::Dialog::foreground}) {}

/* ********************************************************************************************** */

void HelpDialog::Show() {
  ResetSearch();

  first_line_ = 0;
  Open();
}

/* ********************************************************************************************** */

std::vector<HelpDialog::Line> HelpDialog::CreateContent() {
  using keybinding::General;
  using keybinding::Navigation;

  //! Entry from a section: keybinding and its description
  struct Entry {
    std::string keys;
    std::string text;
  };

  //! Section with its title and entries
  struct Content {
    Section section;
    std::string title;
    std::vector<Entry> entries;
  };

  const std::string arrows = "←/↓/↑/→";
  const std::string hjkl =
      Join(Navigation::Left, Navigation::Down) + "/" + Join(Navigation::Up, Navigation::Right);
  const std::string up_down = "↑/↓";
  const std::string jk = Join(Navigation::Down, Navigation::Up);

  const std::vector<Content> sections{
      {Section::General,
       "general",
       {
           {ToString(General::ShowHelper), "Show this help"},
           {ToString(General::ExitApplication), "Quit (or close dialog)"},
           {ToString(General::ChangeTheme),
            "Choose theme (↑/↓ preview, Return keep, Escape cancel)"},
           {ToString(General::ChangeAudioDevice), "Choose audio output device"},
           {"Shift+1", "Focus files/playlists"},
           {"Shift+2", "Focus information"},
           {"Shift+3", "Focus tab viewer"},
           {"Shift+4", "Focus player"},
           {ToString(Navigation::Tab), "Focus next block"},
           {ToString(Navigation::TabReverse), "Focus previous block"},
       }},
      {Section::Lists,
       "lists (files and playlists)",
       {
           {up_down + " " + jk, "Move selection"},
           {Join(Navigation::Home, Navigation::End), "Go to first/last entry"},
           {Join(Navigation::PageUp, Navigation::PageDown), "Go to previous/next page"},
           {ToString(Navigation::EnableSearch), "Search"},
           {ToString(Navigation::Escape), "Exit search"},
       }},
      {Section::Files,
       "files",
       {
           {ToString(keybinding::Sidebar::FocusList), "Show files"},
           {ToString(Navigation::Return), "Enter directory or play song"},
       }},
      {Section::Playlists,
       "playlists",
       {
           {ToString(keybinding::Sidebar::FocusPlaylist), "Show playlists"},
           {ToString(Navigation::Return), "Play playlist (or song from it)"},
           {ToString(Navigation::Space), "Expand/collapse playlist"},
           {"←/→ " + Join(Navigation::Left, Navigation::Right), "Collapse/expand playlist"},
           {ToString(keybinding::Playlist::Create), "Create playlist"},
           {ToString(keybinding::Playlist::Modify), "Modify playlist"},
           {ToString(keybinding::Playlist::Delete), "Delete playlist"},
       }},
      {Section::PlaylistDialog,
       "playlist dialog",
       {
           {Join(Navigation::Tab, Navigation::TabReverse), "Switch between files and playlist"},
           {Join(keybinding::Playlist::ShowFiles, keybinding::Playlist::ShowYoutube),
            "Show files/YouTube URL input"},
           {ToString(Navigation::Space), "Add/remove song"},
           {Join(keybinding::Playlist::RemoveSong, Navigation::Delete),
            "Remove song from playlist"},
           {ToString(Navigation::Return), "Add YouTube URL (while typing it)"},
           {"Ctrl+W/Alt+Backspace", "Delete previous word (while typing)"},
           {ToString(keybinding::Playlist::Rename), "Rename playlist"},
           {ToString(keybinding::Playlist::Save), "Save playlist"},
           {ToString(Navigation::Escape), "Cancel rename or close dialog"},
       }},
      {Section::Visualizer,
       "visualizer",
       {
           {ToString(keybinding::MainContent::FocusVisualizer), "Show visualizer"},
           {ToString(keybinding::Visualizer::ChangeAnimation),
            "Choose animation (↑/↓ preview, Return keep, Escape cancel)"},
           {ToString(keybinding::Visualizer::ToggleFullscreen), "Toggle fullscreen"},
           {Join(keybinding::Visualizer::DecreaseBarWidth,
                 keybinding::Visualizer::IncreaseBarWidth),
            "Decrease/increase bar width"},
       }},
      {Section::Equalizer,
       "equalizer",
       {
           {ToString(keybinding::MainContent::FocusEqualizer), "Show equalizer"},
           {arrows + " " + hjkl, "Navigate and change frequency gain"},
           {Join(Navigation::Space, Navigation::Return), "Open/close preset picker"},
           {jk, "Cycle presets (picker closed)"},
           {ToString(keybinding::Equalizer::ApplyFilters), "Apply equalizer settings"},
           {ToString(keybinding::Equalizer::ResetFilters), "Reset equalizer settings"},
           {ToString(Navigation::Escape), "Close preset picker or remove focus from element"},
       }},
      {Section::Lyrics,
       "lyrics",
       {
           {ToString(keybinding::MainContent::FocusLyric), "Show lyrics"},
           {up_down + " " + jk, "Scroll lyrics"},
           {Join(Navigation::Home, Navigation::End), "Go to beginning/end"},
           {ToString(keybinding::Lyric::Retry), "Retry search (if it failed)"},
       }},
      {Section::Player,
       "player",
       {
           {ToString(keybinding::MediaPlayer::PlayOrPause), "Pause/resume song"},
           {ToString(keybinding::MediaPlayer::Stop), "Stop song"},
           {Join(keybinding::MediaPlayer::VolumeUp, keybinding::MediaPlayer::VolumeDown),
            "Increase/decrease volume"},
           {ToString(keybinding::MediaPlayer::Mute), "Toggle mute"},
           {ToString(keybinding::MediaPlayer::SeekForward), "Seek forward"},
           {ToString(keybinding::MediaPlayer::SeekBackward), "Seek backward"},
           {Join(keybinding::MediaPlayer::SkipToPrevious, keybinding::MediaPlayer::SkipToNext),
            "Skip to previous/next song"},
           {ToString(keybinding::MediaPlayer::ToggleRepeat), "Change repeat mode (off/all/one)"},
           {ToString(keybinding::MediaPlayer::ToggleShuffle), "Toggle shuffle"},
       }},
      {Section::Questions,
       "confirmation dialog",
       {
           {Join(keybinding::Dialog::Yes, keybinding::Dialog::No), "Answer yes/no"},
           {"←/→ " + ToString(Navigation::Tab), "Select answer"},
           {ToString(Navigation::Return), "Confirm selected answer"},
           {ToString(Navigation::Escape), "Close dialog"},
       }},
  };

  // Flatten all sections into lines (each section ends with an empty line)
  std::vector<Line> lines;

  for (const auto& [section, title, entries] : sections) {
    lines.push_back(Line{.type = Line::Type::Title, .text = title, .section = section});

    for (const auto& [keys, text] : entries) {
      lines.push_back(
          Line{.type = Line::Type::Entry, .keys = keys, .text = text, .section = section});
    }

    lines.push_back(Line{.type = Line::Type::Blank, .section = section});
  }

  return lines;
}

/* ********************************************************************************************** */

ftxui::Element HelpDialog::RenderLine(const Line& line) {
  using ftxui::EQUAL;
  using ftxui::WIDTH;

  switch (line.type) {
    case Line::Type::Title:
      return ftxui::text(line.text) | ftxui::color(GetTheme().dialog.text) | ftxui::bold;

    case Line::Type::Entry:
      return ftxui::hbox({
          ftxui::text(line.keys) | ftxui::color(GetTheme().dialog.keybinding) | ftxui::bold |
              ftxui::size(WIDTH, EQUAL, kKeysColumnWidth),
          ftxui::text(line.text) | ftxui::color(GetTheme().dialog.text),
      });

    case Line::Type::Blank:
    default:
      return ftxui::text("");
  }
}

/* ********************************************************************************************** */

ftxui::Element HelpDialog::RenderImpl(const ftxui::Dimensions& curr_size) const {
  // Calculate how many lines fit inside dialog (besides its border, title and scroll hint)
  const int height = CalculateSize(curr_size).dimy;
  visible_lines_ = std::max(1, height - kBorderSize - kHeaderLines - kFooterLines);

  const auto& lines = GetLines();
  const int first = std::clamp(first_line_, 0, GetMaxFirstLine());
  const int last = std::min(first + visible_lines_, static_cast<int>(lines.size()));

  ftxui::Elements content;
  content.reserve(visible_lines_);

  for (int i = first; i < last; i++) {
    content.push_back(RenderLine(lines.at(i)));
  }

  // Let user know that search did not match anything
  if (lines.empty())
    content.push_back(ftxui::text("No matches") | ftxui::color(GetTheme().dialog.text));

  // Let user know where they are and how to scroll (or search)
  const std::string position = lines.empty()
                                   ? "0 of 0"
                                   : std::to_string(first + 1) + "-" + std::to_string(last) +
                                         " of " + std::to_string(lines.size());

  const std::string escape = ToString(keybinding::Navigation::Escape);
  const std::string search = ToString(keybinding::Navigation::EnableSearch);

  std::string hint;
  if (typing_) {
    hint = ToString(keybinding::Navigation::Return) + ": done  " + escape + ": clear";
  } else if (searching_) {
    hint = "↑/↓: scroll  " + search + ": edit search  " + escape + ": clear";
  } else {
    hint = "↑/↓ PgUp/PgDn: scroll  " + search + ": search  " + escape + ": close";
  }

  ftxui::Element status = ftxui::text(position) | ftxui::color(GetTheme().dialog.text);

  if (searching_) {
    status = ftxui::hbox({
        ftxui::text("Search: ") | ftxui::color(GetTheme().dialog.text) | ftxui::bold,
        search_input_.Render(kSearchWidth, typing_),
        ftxui::text("  "),
        status,
    });
  }

  constexpr int kMargin = 3;  //!< Lateral margin for content

  return ftxui::vbox({
      ftxui::text("Help") | ftxui::color(GetTheme().dialog.text) | ftxui::bold | ftxui::center,
      ftxui::text(""),
      ftxui::hbox({
          ftxui::text(std::string(kMargin, ' ')),
          ftxui::vbox(content) | ftxui::flex,
      }) | ftxui::flex,
      ftxui::text(""),
      ftxui::hbox({
          ftxui::text(std::string(kMargin, ' ')),
          status,
          ftxui::filler(),
          ftxui::text(hint) | ftxui::color(GetTheme().dialog.text),
          ftxui::text(std::string(kMargin, ' ')),
      }),
  });
}

/* ********************************************************************************************** */

bool HelpDialog::OnEventImpl(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  if (OnSearchEvent(event)) return true;

  if (event == Keybind::Return) {
    Close();
    return true;
  }

  if (event == Keybind::ArrowUp || event == Keybind::Up) {
    Scroll(-1);
    return true;
  }

  if (event == Keybind::ArrowDown || event == Keybind::Down) {
    Scroll(1);
    return true;
  }

  if (event == Keybind::PageUp) {
    Scroll(-visible_lines_);
    return true;
  }

  if (event == Keybind::PageDown) {
    Scroll(visible_lines_);
    return true;
  }

  if (event == Keybind::Home) {
    first_line_ = 0;
    return true;
  }

  if (event == Keybind::End) {
    first_line_ = GetMaxFirstLine();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

bool HelpDialog::OnMouseEventImpl(ftxui::Event event) {
  if (event.mouse().button == ftxui::Mouse::WheelUp) {
    Scroll(-1);
    return true;
  }

  if (event.mouse().button == ftxui::Mouse::WheelDown) {
    Scroll(1);
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void HelpDialog::Scroll(int offset) {
  first_line_ = std::clamp(first_line_ + offset, 0, GetMaxFirstLine());
}

/* ********************************************************************************************** */

int HelpDialog::GetMaxFirstLine() const {
  return std::max(0, static_cast<int>(GetLines().size()) - visible_lines_);
}

/* ********************************************************************************************** */

bool HelpDialog::OnSearchEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  // Start (or go back to) typing text to search
  if (!typing_ && event == Keybind::EnableSearch) {
    searching_ = true;
    typing_ = true;
    Filter();
    return true;
  }

  if (!searching_) return false;

  // Clear search, showing all content again (and only then, dialog can be closed)
  if (event == Keybind::Escape) {
    ResetSearch();
    return true;
  }

  if (!typing_) return false;

  // Stop typing, but keep content filtered (so it can be scrolled with any keybinding)
  if (event == Keybind::Return) {
    typing_ = false;
    return true;
  }

  // Any other key changes text to search (except for the ones to scroll, handled by caller)
  if (search_input_.OnEvent(event)) {
    Filter();
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

void HelpDialog::Filter() {
  const std::string& text = search_input_.GetText();
  filtered_lines_.clear();
  first_line_ = 0;

  // Content is split into sections: a title, its entries and an empty line
  for (auto title = lines_.begin(); title != lines_.end();) {
    auto next_title = std::find_if(std::next(title), lines_.end(),
                                   [](const Line& line) { return line.type == Line::Type::Title; });

    const bool title_matches = util::contains(title->text, text);
    std::vector<Line> entries;

    for (auto entry = std::next(title); entry != next_title; ++entry) {
      if (entry->type == Line::Type::Entry && (title_matches || util::contains(entry->keys, text) ||
                                               util::contains(entry->text, text))) {
        entries.push_back(*entry);
      }
    }

    if (!entries.empty()) {
      filtered_lines_.push_back(*title);
      filtered_lines_.insert(filtered_lines_.end(), entries.begin(), entries.end());
      filtered_lines_.push_back(Line{.type = Line::Type::Blank, .section = title->section});
    }

    title = next_title;
  }
}

/* ********************************************************************************************** */

void HelpDialog::ResetSearch() {
  searching_ = false;
  typing_ = false;
  search_input_.Clear();
  filtered_lines_.clear();
  first_line_ = 0;
}

}  // namespace interface
