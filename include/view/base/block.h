/**
 * \file
 * \brief  Base class for any content block displayed in the UI
 */

#ifndef INCLUDE_VIEW_BASE_BLOCK_H_
#define INCLUDE_VIEW_BASE_BLOCK_H_

#include <memory>   // for shared_ptr, enable_sha...
#include <string>   // for string, operator==
#include <utility>  // for move

#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component_base.hpp"  // for ComponentBase
#include "ftxui/dom/elements.hpp"              // for Element
#include "view/base/custom_event.h"

namespace interface {

//! Forward declaration
class EventDispatcher;

//! Maximum dimensions for block
struct Size {
  int width, height;
};

/**
 * @brief Base class representing a block in view
 */
class Block : std::enable_shared_from_this<Block>, public ftxui::ComponentBase {
 protected:
  //! Unique ID for each block
  enum class Identifier {
    ListDirectory = 201,
    FileInfo = 202,
    AudioVisualizer = 203,
    MediaPlayer = 204,
    ErrorDialog = 205,
  };

  /**
   * @brief Construct a new Block object (only called by derived classes)
   * @param dispatcher Event dispatcher
   * @param id Unique ID for block
   * @param size Block dimensions
   */
  Block(const std::shared_ptr<EventDispatcher>& dispatcher, const Identifier id, const Size& size);

 public:
  /**
   * @brief Destroy the Block object
   */
  virtual ~Block() = default;

  //! Unique ID
  Identifier GetId() { return id_; }

  //! Block size
  Size GetSize() { return size_; }

  /* ******************************************************************************************** */
  //! These must be implemented by derived class

  virtual ftxui::Element Render() = 0;
  virtual bool OnEvent(ftxui::Event) = 0;
  virtual bool OnCustomEvent(const CustomEvent&) = 0;

  /* ******************************************************************************************** */
  //! Variables
 protected:
  std::weak_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks

 private:
  Identifier id_;  //!< Block identification
  Size size_;      //!< Block size
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_BLOCK_H_
