/**
 * \file
 * \brief  Class representing a single view block
 */

#ifndef INCLUDE_VIEW_BASE_BLOCK_H_
#define INCLUDE_VIEW_BASE_BLOCK_H_

#include <memory>   // for shared_ptr, enable_sha...
#include <string>   // for string, operator==
#include <utility>  // for move

#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component_base.hpp"  // for ComponentBase
#include "ftxui/dom/elements.hpp"              // for Element
#include "view/base/action_listener.h"
#include "view/base/block_event.h"
#include "view/base/event_dispatcher.h"

class ActionListener;

namespace interface {

using BlockIdentifier = uint8_t;

//! Unique ID for each block
static constexpr BlockIdentifier kBlockListDirectory = 201;
static constexpr BlockIdentifier kBlockFileInfo = 202;
static constexpr BlockIdentifier kBlockAudioPlayer = 203;
static constexpr BlockIdentifier kBlockErrorDialog = 204;

/**
 * @brief Base class representing a block in view
 */
class Block : std::enable_shared_from_this<Block>, public ftxui::ComponentBase {
 protected:
  /**
   * @brief Construct a new Block object (only called by derived classes)
   * @param d Dispatcher
   * @param id Unique ID for block
   */
  Block(const std::shared_ptr<EventDispatcher>& d, const BlockIdentifier id);

 public:
  /**
   * @brief Destroy the Block object
   */
  virtual ~Block() = default;

  /* ******************************************************************************************** */
  //! These must be implemented by derived class

  virtual ftxui::Element Render() = 0;
  virtual bool OnEvent(ftxui::Event) = 0;
  virtual void OnBlockEvent(BlockEvent) = 0;

  /* ******************************************************************************************** */
  //! Attach listener to receive action notifications (optional)
  void Attach(const std::shared_ptr<ActionListener>& listener);

  // TODO: evaluate if must exclude this method...
  //! Send an event for other blocks
  void Send(BlockEvent event);

  //! Unique ID
  BlockIdentifier GetId() { return id_; }

  /* ******************************************************************************************** */
 protected:
  BlockIdentifier id_;                           //!< Block identification
  std::shared_ptr<EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  std::shared_ptr<ActionListener> listener_;     //!< Inform actions to outside listener
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_BLOCK_H_