/**
 * \file
 * \brief  Class for block containing main content view
 */

#ifndef INCLUDE_VIEW_BLOCK_MAIN_CONTENT_H_
#define INCLUDE_VIEW_BLOCK_MAIN_CONTENT_H_

#include <memory>

#include "view/base/block.h"
#include "view/element/tab.h"

#ifdef ENABLE_TESTS
namespace {
class MainContentTest;
}
#endif

namespace interface {

/**
 * @brief Component to display a set of tabs and their respective content
 */
class MainContent : public Block {
 public:
  /**
   * @brief Construct a new MainContent object
   * @param dispatcher Block event dispatcher
   */
  explicit MainContent(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the MainContent object
   */
  ~MainContent() override = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Renders the component in fullscreen (without window borders)
   * @return Element Built element based on internal state
   */
  ftxui::Element RenderFullscreen();

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(ftxui::Event event) override;

  /**
   * @brief Handles a custom event
   * @param event Received event (probably sent by Audio thread)
   * @return true if event was handled, otherwise false
   */
  bool OnCustomEvent(const CustomEvent& event) override;

  /**
   * @brief Receives an indication that block is now focused
   */
  void OnFocus() override;

  /**
   * @brief Receives an indication that block is not focused anymore
   */
  void OnLostFocus() override;

  /**
   * @brief Possible tab views to render on this block
   */
  enum View {
    Visualizer,  //!< Display spectrum visualizer (default)
    Equalizer,   //!< Display audio equalizer
    Lyric,       //!< Display song lyric
    LAST,
  };

  /**
   * @brief Get width for a single bar (used for Terminal calculation)
   * @return Audio bar width
   */
  int GetBarWidth();

  /* ******************************************************************************************** */
  //! Private methods
 private:
  //! Handle mouse event
  bool OnMouseEvent(ftxui::Event event);

  //! Create general buttons
  void CreateButtons();

  /* ******************************************************************************************** */
  //! Variables

  //! Buttons located on the upper-right border of block window
  WindowButton btn_help_;  //!< Help button
  WindowButton btn_exit_;  //!< Exit button

  Tab tab_elem_;  //!< Tab containing multiple panels with some content

  bool is_fullscreen_ =
      false;  //!< Cache flag set by parent(Terminal block) via Render or RenderFullscreen, this is
              //!< used later by OnEvent to decide if could process event or not

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::MainContentTest;
#endif
};

}  // namespace interface

#endif  // INCLUDE_VIEW_BLOCK_MAIN_CONTENT_H_
