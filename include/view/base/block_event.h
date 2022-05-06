/**
 * \file
 * \brief  Class representing a block event
 */

#ifndef INCLUDE_VIEW_BASE_BLOCK_EVENT_H_
#define INCLUDE_VIEW_BASE_BLOCK_EVENT_H_

#include <string>

namespace interface {

//! Shared events between blocks
struct BlockEvent {
  // TODO: implement custom events with content (check event.hpp/cpp from ftxui)
  static BlockEvent Special(std::string);

  //! Overload operators
  bool operator==(const BlockEvent& other) const { return type_ == other.type_; }
  bool operator!=(const BlockEvent& other) const { return !operator==(other); }

  //! Setter and getter
  void SetContent(const std::string& content) { content_ = std::move(content); }
  const std::string& Content() const { return content_; }

  const bool IsEmpty() const { return content_.size() == 0; }

  //! Possible events
  static BlockEvent UpdateFileInfo;

 private:
  std::string type_;     // Event name
  std::string content_;  // Content to send through event (optional)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_BLOCK_EVENT_H_
