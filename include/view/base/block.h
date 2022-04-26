/**
 * \file
 * \brief  Class representing a single view block
 */

#ifndef INCLUDE_UI_BASE_BLOCK_H_
#define INCLUDE_UI_BASE_BLOCK_H_

#include <memory>   // for shared_ptr, enable_sha...
#include <string>   // for string, operator==
#include <utility>  // for move

#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component_base.hpp"  // for ComponentBase
#include "ftxui/dom/elements.hpp"              // for Element
#include "view/base/block_event.h"

namespace interface {

using namespace ftxui;

//! Unique ID for each block
constexpr int kBlockListDirectory = 301;
constexpr int kBlockFileInfo = 302;

class Dispatcher;  //!< Forward declaration

/**
 * @brief Base class representing a block in view
 */
class Block : std::enable_shared_from_this<Block>, public ComponentBase {
 protected:
  /**
   * @brief Construct a new Block object
   * @param d Dispatcher
   * @param id Unique ID for block
   */
  Block(const std::shared_ptr<Dispatcher>& d, const unsigned int id);

 public:
  /**
   * @brief Destroy the Block object
   */
  virtual ~Block() = default;

  /* ******************************************************************************************** */
  //! These must be implemented by derived class

  virtual Element Render() = 0;
  virtual bool OnEvent(Event) = 0;
  virtual void OnBlockEvent(BlockEvent) = 0;

  /* ******************************************************************************************** */
  //! Send a block event for other blocks
  void Send(BlockEvent);

  //! Unique ID
  unsigned int GetId() { return id_; }

  /* ******************************************************************************************** */
 protected:
  std::shared_ptr<Dispatcher> dispatcher_;  //!< Dispatch events for other blocks
  unsigned int id_;                         //!< Block identification
};

}  // namespace interface
#endif  // INCLUDE_UI_BASE_BLOCK_H_