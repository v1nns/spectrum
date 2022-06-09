/**
 * \file
 * \brief Structure for a custom event
 */

#ifndef INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_
#define INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_

#include <variant>

#include "model/song.h"

namespace interface {

/**
 * @brief Interface for custom events to be handled by blocks
 */
struct CustomEvent {
  //! Custom event identifier TODO: fix to remove 'k'
  static constexpr int kUpdateFileInfo = 50000;
  static constexpr int kClearFileInfo = 50001;

  //! Overloaded operators
  bool operator==(const int other) const { return identifier_ == other; }
  bool operator!=(const int other) const { return !operator==(other); }

 public:
  //! Possible events
  static CustomEvent UpdateFileInfo(const model::Song& info);
  static CustomEvent ClearFileInfo();

  //! Custom getter for each possible event
  model::Song GetContent() const;

 private:
  //! Possible types for content
  using Content = std::variant<model::Song>;

  //! Variables
  int identifier_;   //!< Unique identifier for Event
  Content content_;  //!< Wrapper for content
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_