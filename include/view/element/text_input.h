/**
 * \file
 * \brief  Class for editing a single line of text
 */

#ifndef INCLUDE_VIEW_ELEMENT_TEXT_INPUT_H_
#define INCLUDE_VIEW_ELEMENT_TEXT_INPUT_H_

#include <functional>
#include <string>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace interface {

/**
 * @brief Single line text input, handling text as UTF-8 glyphs (so cursor never splits a
 * character), with horizontal scroll to keep cursor always visible
 */
class TextInput {
 public:
  /**
   * @brief Filter for typed characters
   * @param character UTF-8 character typed by user
   * @return true if character can be inserted, false otherwise
   */
  using Filter = std::function<bool(const std::string& character)>;

  /**
   * @brief Construct a new TextInput object
   * @param filter Filter for typed characters (by default, accept all of them)
   */
  explicit TextInput(Filter filter = nullptr);

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Handles an editing event: characters, Backspace, Delete, ←/→, Home, End and delete
   * previous word (Ctrl+Backspace, Ctrl+W or Alt+Backspace)
   * @param event Received event from screen
   * @return true if event was handled (characters are always handled, even if filtered out)
   */
  bool OnEvent(const ftxui::Event& event);

  /**
   * @brief Renders text input as a field with fixed width
   * @param width Number of columns for field
   * @param show_cursor Flag to render cursor (scrolling text to keep it visible)
   * @param placeholder Dimmed text displayed while text is empty
   * @return Element Built element based on internal state
   */
  ftxui::Element Render(int width, bool show_cursor, const std::string& placeholder = "") const;

  /**
   * @brief Set text content (placing cursor after its last character)
   * @param text New text
   */
  void SetText(const std::string& text);

  /**
   * @brief Clear text content
   */
  void Clear() { SetText(""); }

  //! Getter for text content
  const std::string& GetText() const { return text_; }

  //! Check if text content is empty
  bool IsEmpty() const { return text_.empty(); }

  /* ******************************************************************************************** */
  //! Internal implementation
 private:
  //! Split text content into glyphs
  std::vector<std::string> GetGlyphs() const;

  //! Join glyphs back into text content
  void SetGlyphs(const std::vector<std::string>& glyphs);

  //! Delete word before cursor (and any separators between it and the cursor), like a shell does
  void DeletePreviousWord();

  /* ******************************************************************************************** */
  //! Variables

  Filter filter_;     //!< Filter for typed characters
  std::string text_;  //!< Text content
  int cursor_ = 0;    //!< Cursor position (as glyph index)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_TEXT_INPUT_H_
