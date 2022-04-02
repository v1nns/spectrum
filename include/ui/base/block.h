/**
 * \file
 * \brief  Class representing a single UI block
 */

#ifndef INCLUDE_UI_BASE_BLOCK_H_
#define INCLUDE_UI_BASE_BLOCK_H_

#include <memory>

#include "ftxui/component/component_base.hpp"  // for Component

namespace interface {

//! Unique ID for each block
constexpr int kBlockListDirectory = 301;
constexpr int kBlockFileInfo = 302;

//! Shared events between blocks
struct BlockEvent {
  // TODO: implement custom events with content (check event.hpp/cpp from ftxui)
  static BlockEvent Special(std::string);

  static BlockEvent FileSelected;

  bool operator==(const BlockEvent& other) const { return type_ == other.type_; }
  bool operator!=(const BlockEvent& other) const { return !operator==(other); }

  void SetContent(const std::string& content) { content_ = std::move(content); }

 private:
  std::string type_;
  std::string content_;
};

/* ********************************************************************************************** */

using namespace ftxui;

class Dispatcher;  //!< Forward declaration

/**
 * @brief Base class representing a block in UI
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