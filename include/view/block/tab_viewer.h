/**
 * \file
 * \brief  Class for block containing tab view
 */

#ifndef INCLUDE_VIEW_BLOCK_TAB_VIEWER_H_
#define INCLUDE_VIEW_BLOCK_TAB_VIEWER_H_

#include <memory>
#include <unordered_map>

#include "audio/lyric/base/html_parser.h"
#include "audio/lyric/base/url_fetcher.h"
#include "view/base/block.h"
#include "view/element/button.h"
#include "view/element/tab_item.h"

#ifdef ENABLE_TESTS
namespace {
class TabViewerTest;
}
#endif

namespace interface {

/**
 * @brief Component to display a set of tabs and their respective content
 */
class TabViewer : public Block {
  //! For readability
  using Item = std::unique_ptr<TabItem>;
  using Keybinding = std::string;

 public:
  /**
   * @brief Construct a new TabViewer object
   * @param dispatcher Block event dispatcher
   */
  explicit TabViewer(const std::shared_ptr<EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the TabViewer object
   */
  ~TabViewer() override = default;

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
  enum class View {
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

  //! Get active tabview
  Item& active() { return views_[active_].item; }

  //! Get active tabview
  WindowButton& active_button() { return views_[active_].button; }

  //! Create window buttons
  void CreateButtons();

  //! Create all tab views
  void CreateViews(const std::shared_ptr<EventDispatcher>& dispatcher);

  /* ******************************************************************************************** */
  //! Variables

  //! Buttons located on the upper-right border of block window
  WindowButton btn_help_;  //!< Help button
  WindowButton btn_exit_;  //!< Exit button

  //! Represent a single tab item entry
  struct Tab {
    Keybinding key;       //!< Keybinding to set item as active
    WindowButton button;  //!< Button to render in the tab border
    Item item;            //!< View to render in the tab content
  };

  View active_ = View::Visualizer;       //!< Current view displayed on block
  std::unordered_map<View, Tab> views_;  //!< All possible views to render in this component

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::TabViewerTest;
#endif
};

}  // namespace interface

#endif  // INCLUDE_VIEW_BLOCK_TAB_VIEWER_H_
