#include "view/element/style.h"

#include <array>
#include <cstdint>

namespace interface {

namespace {

//! Color kept as RGB, so other colors can be derived from it
struct Rgb {
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  //! Any color from theme can be filled directly from palette
  operator ftxui::Color() const { return ftxui::Color::RGB(red, green, blue); }  // NOLINT
};

/* ********************************************************************************************** */

//! Mix two colors (ratio goes from 0.0, only first color, to 1.0, only second color)
Rgb Mix(const Rgb& first, const Rgb& second, float ratio) {
  auto mix = [ratio](uint8_t a, uint8_t b) {
    return static_cast<uint8_t>(static_cast<float>(a) * (1.F - ratio) +
                                static_cast<float>(b) * ratio);
  };

  return Rgb{mix(first.red, second.red), mix(first.green, second.green),
             mix(first.blue, second.blue)};
}

/* ********************************************************************************************** */

//! Colors from a color scheme, with the role each one takes in UI
struct Palette {
  Rgb base;     //!< Darkest background, also used for text placed over an accent color
  Rgb surface;  //!< Background for dialogs and block titles
  Rgb overlay;  //!< Borders without focus, empty part from bars and disabled elements
  Rgb muted;    //!< Hints, placeholders and anything else that should not stand out
  Rgb subtext;  //!< Secondary text
  Rgb text;     //!< Main text

  Rgb accent;      //!< Focused and main elements
  Rgb accent_alt;  //!< Hovered elements and keybindings

  Rgb green;
  Rgb red;
  Rgb yellow;
  Rgb special;  //!< Tags and anything else that needs a color of its own

  std::array<Rgb, 4> gradient;  //!< Spectrum, from the lowest to the highest part
};

/* ********************************************************************************************** */

//! Create theme by filling every color with one from palette
Theme MakeTheme(const Palette& p) {
  using State = Theme::State;
  using ButtonStates = Theme::ButtonStates;

  //! Positions for colors from spectrum gradient
  static constexpr std::array<float, 4> kGradientPositions{0.0F, 0.3F, 0.6F, 0.8F};

  // Red that can still be read when placed over an accent color
  const Rgb dark_red = Mix(p.red, p.base, 0.6F);

  Theme theme;

  theme.block = Theme::Block{
      .title = State{.foreground = p.subtext, .background = p.surface},
      .title_focused = State{.foreground = p.base, .background = p.accent},
      .border_focused = p.accent,
      .tab =
          ButtonStates{
              .normal = State{.foreground = p.muted},
              .focused = State{.foreground = p.subtext, .background = p.surface},
              .selected = State{.foreground = p.base, .background = p.accent},
          },
      .window_button =
          ButtonStates{
              .focused = State{.foreground = p.subtext, .background = p.surface},
              .pressed = State{.foreground = p.text, .background = p.overlay},
          },
  };

  theme.menu = Theme::Menu{
      .prefix = p.accent,
      .title = p.text,
      .search = p.text,
      .directory = p.green,
      .file = p.text,
      .file_playing = p.accent,
      .playlist = p.accent,
      .playlist_playing = p.green,
      .song = p.text,
      .song_playing = p.accent_alt,
  };

  theme.sidebar = Theme::Sidebar{
      .button =
          ButtonStates{
              .normal = State{.foreground = p.base, .background = p.accent},
              .focused = State{.foreground = p.base, .background = p.accent_alt},
              .pressed = State{.foreground = p.accent_alt, .background = p.overlay},
              .disabled = State{.foreground = p.muted, .background = p.surface},
              .highlight = State{.foreground = dark_red},
          },
  };

  theme.file_info = Theme::FileInfo{
      .field = p.accent,
      .value = p.text,
      .value_empty = p.muted,
  };

  theme.visualizer.text = p.text;

  for (size_t i = 0; i < theme.visualizer.gradient.size(); i++) {
    const Rgb& color = p.gradient.at(i);
    theme.visualizer.gradient.at(i) =
        Theme::ColorStop{kGradientPositions.at(i), color.red, color.green, color.blue};
  }

  theme.equalizer = Theme::Equalizer{
      .text = p.text,
      .bar = State{.foreground = p.accent, .background = p.overlay},
      .bar_hovered = State{.foreground = p.accent_alt, .background = p.muted},
      .bar_focused = State{.foreground = p.red, .background = p.muted},
      .button =
          ButtonStates{
              .normal = State{.foreground = p.text, .border = p.text},
              .focused = State{.border = p.accent},
              .pressed = State{.foreground = p.accent, .background = p.surface, .border = p.accent},
              .disabled = State{.foreground = p.muted, .border = p.muted},
              .highlight = State{.foreground = p.red},
          },
  };

  theme.lyric.text = p.text;

  theme.player = Theme::Player{
      .text = p.text,
      .volume_muted = p.red,
      .warning = p.yellow,
      .play = p.green,
      .stop = p.red,
      .skip = p.accent,
      .button_border = p.overlay,
      .button_border_focused = p.accent,
      .duration = State{.foreground = p.accent, .background = p.overlay},
      .duration_focused = State{.foreground = p.accent_alt, .background = p.muted},
  };

  theme.dialog = Theme::Dialog{
      .border = p.accent,
      .background = p.surface,
      .background_error = Mix(p.base, p.red, 0.3F),
      .foreground = p.text,
      .text = p.text,
      .label = p.text,
      .hint = p.subtext,
      .keybinding = p.accent_alt,
      .success = p.green,
      .error = p.red,
      .pane_border = p.muted,
      .pane_border_focused = p.accent,
      .pane_title = p.text,
      .input = State{.foreground = p.text, .background = p.base},
      .input_placeholder = p.muted,
      .menu_directory = p.green,
      .menu_file = p.text,
      .menu_file_playing = p.accent,
      .menu_song = p.text,
      .menu_tag = p.special,
      .button =
          ButtonStates{
              .normal = State{.foreground = p.base, .background = p.accent, .border = p.muted},
              .focused =
                  State{.foreground = p.base, .background = p.accent_alt, .border = p.accent_alt},
              .pressed = State{.foreground = p.accent, .background = p.overlay, .border = p.accent},
              .disabled = State{.foreground = p.muted, .background = p.overlay, .border = p.muted},
          },
      .tab =
          ButtonStates{
              .normal = State{.foreground = p.subtext, .background = p.surface},
              .focused = State{.foreground = p.base, .background = p.accent_alt},
              .selected = State{.foreground = p.base, .background = p.accent},
          },
      .answer =
          ButtonStates{
              .normal = State{.foreground = p.base, .background = p.accent},
              .focused = State{.foreground = p.base, .background = p.accent_alt},
              .selected = State{.foreground = p.base, .background = p.text},
              .pressed = State{.foreground = p.accent_alt, .background = p.overlay},
              .highlight = State{.foreground = dark_red},
          },
  };

  theme.picker = Theme::Picker{
      .border = p.accent,
      .entry = p.subtext,
      .entry_selected = p.text,
  };

  return theme;
}

/* ********************************************************************************************** */

//! Tokyo Night (https://github.com/folke/tokyonight.nvim)
const Palette kTokyoNight{
    .base = {0x1A, 0x1B, 0x26},
    .surface = {0x29, 0x2E, 0x42},
    .overlay = {0x41, 0x48, 0x68},
    .muted = {0x56, 0x5F, 0x89},
    .subtext = {0xA9, 0xB1, 0xD6},
    .text = {0xC0, 0xCA, 0xF5},
    .accent = {0x7A, 0xA2, 0xF7},
    .accent_alt = {0x7D, 0xCF, 0xFF},
    .green = {0x9E, 0xCE, 0x6A},
    .red = {0xF7, 0x76, 0x8E},
    .yellow = {0xE0, 0xAF, 0x68},
    .special = {0xBB, 0x9A, 0xF7},
    .gradient = {{{0x7A, 0xA2, 0xF7}, {0x7D, 0xCF, 0xFF}, {0xBB, 0x9A, 0xF7}, {0xF7, 0x76, 0x8E}}},
};

//! Catppuccin Mocha (https://catppuccin.com/palette)
const Palette kCatppuccinMocha{
    .base = {0x1E, 0x1E, 0x2E},
    .surface = {0x31, 0x32, 0x44},
    .overlay = {0x45, 0x47, 0x5A},
    .muted = {0x6C, 0x70, 0x86},
    .subtext = {0xA6, 0xAD, 0xC8},
    .text = {0xCD, 0xD6, 0xF4},
    .accent = {0xCB, 0xA6, 0xF7},
    .accent_alt = {0xF5, 0xC2, 0xE7},
    .green = {0xA6, 0xE3, 0xA1},
    .red = {0xF3, 0x8B, 0xA8},
    .yellow = {0xF9, 0xE2, 0xAF},
    .special = {0xFA, 0xB3, 0x87},
    .gradient = {{{0x89, 0xB4, 0xFA}, {0xB4, 0xBE, 0xFE}, {0xCB, 0xA6, 0xF7}, {0xF5, 0xC2, 0xE7}}},
};

//! Gruvbox Dark (https://github.com/morhetz/gruvbox)
const Palette kGruvboxDark{
    .base = {0x28, 0x28, 0x28},
    .surface = {0x3C, 0x38, 0x36},
    .overlay = {0x50, 0x49, 0x45},
    .muted = {0x7C, 0x6F, 0x64},
    .subtext = {0xA8, 0x99, 0x84},
    .text = {0xEB, 0xDB, 0xB2},
    .accent = {0xFE, 0x80, 0x19},
    .accent_alt = {0xFA, 0xBD, 0x2F},
    .green = {0xB8, 0xBB, 0x26},
    .red = {0xFB, 0x49, 0x34},
    .yellow = {0xFA, 0xBD, 0x2F},
    .special = {0xD3, 0x86, 0x9B},
    .gradient = {{{0xB8, 0xBB, 0x26}, {0xFA, 0xBD, 0x2F}, {0xFE, 0x80, 0x19}, {0xFB, 0x49, 0x34}}},
};

//! Nord (https://www.nordtheme.com)
const Palette kNord{
    .base = {0x2E, 0x34, 0x40},
    .surface = {0x3B, 0x42, 0x52},
    .overlay = {0x4C, 0x56, 0x6A},
    .muted = {0x61, 0x6E, 0x88},
    .subtext = {0xD8, 0xDE, 0xE9},
    .text = {0xEC, 0xEF, 0xF4},
    .accent = {0x81, 0xA1, 0xC1},
    .accent_alt = {0x88, 0xC0, 0xD0},
    .green = {0xA3, 0xBE, 0x8C},
    .red = {0xBF, 0x61, 0x6A},
    .yellow = {0xEB, 0xCB, 0x8B},
    .special = {0xB4, 0x8E, 0xAD},
    .gradient = {{{0x5E, 0x81, 0xAC}, {0x81, 0xA1, 0xC1}, {0x88, 0xC0, 0xD0}, {0x8F, 0xBC, 0xBB}}},
};

//! Dracula (https://draculatheme.com)
const Palette kDracula{
    .base = {0x28, 0x2A, 0x36},
    .surface = {0x34, 0x37, 0x46},
    .overlay = {0x44, 0x47, 0x5A},
    .muted = {0x62, 0x72, 0xA4},
    .subtext = {0xBF, 0xC1, 0xD0},
    .text = {0xF8, 0xF8, 0xF2},
    .accent = {0xBD, 0x93, 0xF9},
    .accent_alt = {0xFF, 0x79, 0xC6},
    .green = {0x50, 0xFA, 0x7B},
    .red = {0xFF, 0x55, 0x55},
    .yellow = {0xF1, 0xFA, 0x8C},
    .special = {0xFF, 0xB8, 0x6C},
    .gradient = {{{0x8B, 0xE9, 0xFD}, {0xBD, 0x93, 0xF9}, {0xFF, 0x79, 0xC6}, {0xFF, 0xB8, 0x6C}}},
};

}  // namespace

/* ********************************************************************************************** */

const std::vector<ThemeOption>& GetThemes() {
  static const std::vector<ThemeOption> themes{
      {"tokyo-night", "Tokyo Night", MakeTheme(kTokyoNight)},
      {"catppuccin-mocha", "Catppuccin Mocha", MakeTheme(kCatppuccinMocha)},
      {"gruvbox-dark", "Gruvbox Dark", MakeTheme(kGruvboxDark)},
      {"nord", "Nord", MakeTheme(kNord)},
      {"dracula", "Dracula", MakeTheme(kDracula)},
  };

  return themes;
}

}  // namespace interface
