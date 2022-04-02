/**
 * \file
 * \brief  Class representing a block event
 */

#ifndef INCLUDE_UI_BASE_BLOCK_EVENT_H_
#define INCLUDE_UI_BASE_BLOCK_EVENT_H_

#include <string>

namespace interface {

//! Shared events between blocks
struct BlockEvent {
  // TODO: implement custom events with content (check event.hpp/cpp from ftxui)
  static BlockEvent Special(std::string);

  static BlockEvent FileSelected;

  bool operator==(const BlockEvent& other) const { return type_ == other.type_; }
  bool operator!=(const BlockEvent& other) const { return !operator==(other); }

  void SetContent(const std::string& content) { content_ = std::move(content); }
  const std::string& Content() const { return content_; }

 private:
  std::string type_;
  std::string content_;
};

}  // namespace interface
#endif  // INCLUDE_UI_BASE_BLOCK_EVENT_H_
