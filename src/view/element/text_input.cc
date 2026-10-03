#include "view/element/text_input.h"

#include <algorithm>
#include <cctype>
#include <numeric>

#include "ftxui/screen/string.hpp"
#include "view/base/keybinding.h"
#include "view/element/style.h"

namespace interface {

TextInput::TextInput(Filter filter) : filter_{std::move(filter)} {}

/* ********************************************************************************************** */

bool TextInput::OnEvent(const ftxui::Event& event) {
  using Keybind = keybinding::Navigation;

  auto glyphs = GetGlyphs();
  int size = static_cast<int>(glyphs.size());

  if (event.is_character()) {
    const std::string& character = event.character();
    if (filter_ && !filter_(character)) return true;

    // Insert as text (instead of glyph), so combining characters merge with the previous glyph
    std::string before = std::accumulate(glyphs.begin(), glyphs.begin() + cursor_, std::string{});
    std::string after = std::accumulate(glyphs.begin() + cursor_, glyphs.end(), std::string{});

    text_ = before + character + after;
    cursor_ += static_cast<int>(GetGlyphs().size()) - size;
    return true;
  }

  if (event == Keybind::Backspace) {
    if (cursor_ > 0) {
      glyphs.erase(glyphs.begin() + cursor_ - 1);
      cursor_--;
      SetGlyphs(glyphs);
    }
    return true;
  }

  if (event == Keybind::CtrlBackspace || event == Keybind::CtrlW ||
      event == Keybind::AltBackspace) {
    DeletePreviousWord();
    return true;
  }

  if (event == Keybind::Delete) {
    if (cursor_ < size) {
      glyphs.erase(glyphs.begin() + cursor_);
      SetGlyphs(glyphs);
    }
    return true;
  }

  if (event == Keybind::ArrowLeft) {
    cursor_ = std::max(cursor_ - 1, 0);
    return true;
  }

  if (event == Keybind::ArrowRight) {
    cursor_ = std::min(cursor_ + 1, size);
    return true;
  }

  if (event == Keybind::Home) {
    cursor_ = 0;
    return true;
  }

  if (event == Keybind::End) {
    cursor_ = size;
    return true;
  }

  return false;
}

/* ********************************************************************************************** */

ftxui::Element TextInput::Render(int width, bool show_cursor,
                                 const std::string& placeholder) const {
  const auto& theme = GetTheme().dialog;

  auto field = ftxui::bgcolor(theme.input.background) | ftxui::color(theme.input.foreground) |
               ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);

  auto cursor = [show_cursor](const std::string& glyph) {
    return show_cursor ? ftxui::text(glyph) | ftxui::inverted : ftxui::text(glyph);
  };

  // Fill remaining columns with spaces (instead of a filler), otherwise whatever was drawn below
  // the field (e.g. a window border) would still be visible
  auto padding = [width](int used) {
    return ftxui::text(std::string(std::max(width - used, 0), ' '));
  };

  if (text_.empty()) {
    int cursor_width = show_cursor ? 1 : 0;

    return ftxui::hbox({
               show_cursor ? cursor(" ") : ftxui::emptyElement(),
               ftxui::text(placeholder) | ftxui::color(theme.input_placeholder),
               padding(cursor_width + ftxui::string_width(placeholder)),
           }) |
           field;
  }

  if (!show_cursor) {
    return ftxui::hbox({ftxui::text(text_), padding(ftxui::string_width(text_))}) | field;
  }

  auto glyphs = GetGlyphs();
  int size = static_cast<int>(glyphs.size());

  // Scroll text horizontally, so cursor is always visible (it uses one extra column when it is
  // placed after the last character)
  auto columns = [&glyphs](int begin, int end) {
    int sum = 0;
    for (int i = begin; i < end; ++i) sum += ftxui::string_width(glyphs[static_cast<size_t>(i)]);
    return sum;
  };

  int cursor_width = cursor_ < size ? ftxui::string_width(glyphs[static_cast<size_t>(cursor_)]) : 1;

  int start = 0;
  while (start < cursor_ && columns(start, cursor_) + cursor_width > width) start++;

  auto join = [&glyphs](int begin, int end) {
    return std::accumulate(glyphs.begin() + begin, glyphs.begin() + end, std::string{});
  };

  return ftxui::hbox({
             ftxui::text(join(start, cursor_)),
             cursor(cursor_ < size ? glyphs[static_cast<size_t>(cursor_)] : " "),
             ftxui::text(cursor_ < size ? join(cursor_ + 1, size) : ""),
             padding(columns(start, size) + (cursor_ < size ? 0 : 1)),
         }) |
         field;
}

/* ********************************************************************************************** */

void TextInput::SetText(const std::string& text) {
  text_ = text;
  cursor_ = static_cast<int>(GetGlyphs().size());
}

/* ********************************************************************************************** */

std::vector<std::string> TextInput::GetGlyphs() const {
  auto glyphs = ftxui::Utf8ToGlyphs(text_);

  // Full-width characters are followed by an empty glyph (only used to reserve a cell on screen)
  glyphs.erase(std::remove(glyphs.begin(), glyphs.end(), ""), glyphs.end());

  return glyphs;
}

/* ********************************************************************************************** */

void TextInput::SetGlyphs(const std::vector<std::string>& glyphs) {
  text_ = std::accumulate(glyphs.begin(), glyphs.end(), std::string{});
}

/* ********************************************************************************************** */

void TextInput::DeletePreviousWord() {
  auto glyphs = GetGlyphs();

  // Letters and digits (including multi-byte ones, like accented letters) are part of a word, while
  // anything else is a separator (e.g. space, slash or dot)
  auto is_word = [](const std::string& glyph) {
    return glyph.size() > 1 || std::isalnum(static_cast<unsigned char>(glyph.front()));
  };

  int begin = cursor_;
  while (begin > 0 && !is_word(glyphs[static_cast<size_t>(begin - 1)])) begin--;
  while (begin > 0 && is_word(glyphs[static_cast<size_t>(begin - 1)])) begin--;

  glyphs.erase(glyphs.begin() + begin, glyphs.begin() + cursor_);
  cursor_ = begin;
  SetGlyphs(glyphs);
}

}  // namespace interface
