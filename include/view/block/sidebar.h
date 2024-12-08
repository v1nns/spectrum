/**
 * \file
 * \brief  Class for block containing sidebar view
 */

#ifndef INCLUDE_VIEW_BLOCK_SIDEBAR_H_
#define INCLUDE_VIEW_BLOCK_SIDEBAR_H_

#include <memory>

#include "util/file_handler.h"
#include "view/base/block.h"
#include "view/element/tab.h"

#ifdef ENABLE_TESTS
namespace {
class SidebarTest;
}
#endif

namespace interface {

/**
 * @brief Component to display a set of tabs and their respective content in the sidebar
 */
class Sidebar : public Block {
  static constexpr int kMaxColumns = 36;  //!< Maximum columns for Component

 public:
  /**
   * @brief Construct a new Sidebar object
   * @param dispatcher Block event dispatcher
   * @param optional_path List files from custom path instead of the current one
   * @param file_handler Interface to file handler
   */
  explicit Sidebar(const std::shared_ptr<EventDispatcher>& dispatcher,
                   const std::string& optional_path = "",
                   const std::shared_ptr<util::FileHandler> file_handler = nullptr);

  /**
   * @brief Destroy the Sidebar object
   */
  ~Sidebar() override = default;

  /**
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

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
    Files,     //!< Display file list from directory (default)
    Playlist,  //!< Display playlist viewer
    LAST,
  };

  /* ******************************************************************************************** */
  //! Private methods
 private:
  //! Handle mouse event
  bool OnMouseEvent(ftxui::Event event);

  /* ******************************************************************************************** */
  //! Variables

  Tab tab_elem_;  //!< Tab containing multiple panels with some content

  //!< Utility class to manage files (read/write)
  std::shared_ptr<util::FileHandler> file_handler_;

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::SidebarTest;
#endif
};

}  // namespace interface

#endif  // INCLUDE_VIEW_BLOCK_SIDEBAR_H_
