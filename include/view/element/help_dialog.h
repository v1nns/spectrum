/**
 * \file
 * \brief  Class for rendering a customized menu helper
 */

#ifndef INCLUDE_VIEW_ELEMENT_HELP_H_
#define INCLUDE_VIEW_ELEMENT_HELP_H_

#include <cstdint>
#include <string>
#include <vector>

#include "view/base/dialog.h"

namespace interface {

/**
 * @brief Customized dialog box to show all keybindings, split by sections and scrollable
 */
class HelpDialog : public Dialog {
  static constexpr int kMaxColumns = 90;       //!< Width for Element
  static constexpr float kHeightRatio = 0.8F;  //!< Height relative to terminal height
  static constexpr int kMinLines = 12;         //!< Minimum lines for Element
  static constexpr int kMaxLines = 40;         //!< Maximum lines for Element

  static constexpr int kHeaderLines = 2;       //!< Lines used by title (and margin below it)
  static constexpr int kFooterLines = 2;       //!< Lines used by scroll hint (and margin above it)
  static constexpr int kKeysColumnWidth = 20;  //!< Width for column with keybindings

 public:
  //! Sections from help, each one describing keybindings for a part of the interface
  enum class Section : uint8_t {
    General,         //!< Keybindings available everywhere
    Lists,           //!< Navigation on lists (files and playlists)
    Files,           //!< Sidebar with files
    Playlists,       //!< Sidebar with playlists
    PlaylistDialog,  //!< Dialog to create/modify a playlist
    Visualizer,      //!< Spectrum visualizer
    Equalizer,       //!< Audio equalizer
    Lyrics,          //!< Song lyrics
    Player,          //!< Media player
    Questions,       //!< Dialog asking for confirmation
  };

  /**
   * @brief Construct a new Help object
   * @param dispatcher Event dispatcher
   */
  explicit HelpDialog(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy Help object
   */
  ~HelpDialog() override = default;

  /**
   * @brief Show help, scrolled to the given section
   * @param section Section to show first (e.g. related to the focused block)
   */
  void Show(Section section);

  /* ******************************************************************************************** */
  //! Custom implementation
 private:
  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element RenderImpl(const ftxui::Dimensions& curr_size) const override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEventImpl(const ftxui::Event& event) override;

  /**
   * @brief Handles an event (from mouse)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnMouseEventImpl(ftxui::Event event) override;

  /* ******************************************************************************************** */
  //! Content
 private:
  //! Single line from help content
  struct Line {
    //! Possible types of line
    enum class Type : uint8_t { Title, Entry, Blank };

    Type type = Type::Blank;  //!< Line type
    std::string keys;         //!< Keybindings (only for entries)
    std::string text;         //!< Section title or keybinding description
    Section section;          //!< Section that contains this line
  };

  /**
   * @brief Create all lines from help content
   * @return Help content
   */
  static std::vector<Line> CreateContent();

  /**
   * @brief Render a single line from help content
   * @param line Line to render
   * @return User interface element
   */
  static ftxui::Element RenderLine(const Line& line);

  /**
   * @brief Scroll content, keeping it within limits
   * @param offset Number of lines to scroll (negative values scroll up)
   */
  void Scroll(int offset);

  //! Get maximum value for first line visible
  int GetMaxFirstLine() const;

  /* ******************************************************************************************** */
  //! Variables

  std::vector<Line> lines_ = CreateContent();  //!< Help content
  int first_line_ = 0;                         //!< First line visible
  mutable int visible_lines_ = 1;              //!< Lines visible (updated when rendered)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_HELP_H_
