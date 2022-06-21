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
  //! Identifier for all existing events
  enum class Type {
    ClearSongInfo = 50000,
    UpdateSongInfo = 50001,
    UpdateSongState = 50002,
  };

  //! Overloaded operators
  bool operator==(const Type other) const { return type_ == other; }
  bool operator!=(const Type other) const { return !operator==(other); }

 public:
  //! Possible events
  static CustomEvent ClearSongInfo();
  static CustomEvent UpdateSongInfo(const model::Song& info);
  static CustomEvent UpdateSongState(const model::Song::State& new_state);

  //! Generic getter for event content
  template <typename T>
  T GetContent() const {
    if (std::holds_alternative<T>(content_)) {
      return std::get<T>(content_);
    } else {
      return T();
    }
  }

 private:
  //! Possible types for content
  using Content = std::variant<model::Song, model::Song::State>;

  //! Variables
  Type type_;        //!< Unique type identifier for Event
  Content content_;  //!< Wrapper for content
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_