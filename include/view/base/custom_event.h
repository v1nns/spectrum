/**
 * \file
 * \brief Structure for a custom event
 */

#ifndef INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_
#define INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_

#include <filesystem>
#include <variant>
#include <vector>

#include "model/song.h"

namespace interface {

/**
 * @brief Interface for custom events to be handled by blocks
 */
struct CustomEvent {
  // Possible group type events (it determines event direction)
  enum class Type {
    FromInterfaceToAudioThread = 40000,
    FromAudioThreadToInterface = 40001,
  };

  //! Identifier for all existing events
  enum class Identifier {
    // Events between blocks TODO: add a better documentation for each one
    ClearSongInfo = 50000,
    UpdateSongInfo = 50001,
    UpdateSongState = 50002,
    DrawAudioRaw = 50003,
    // Events for an outside listener (in this case, audio player thread)
    NotifyFileSelection = 60000,
    PauseOrResumeSong = 60001,
    ClearCurrentSong = 60002,
  };

  //! Overloaded operators
  bool operator==(const Identifier other) const { return id == other; }
  bool operator!=(const Identifier other) const { return !operator==(other); }

 public:
  //! Possible events (from audio thread to interface)
  static CustomEvent ClearSongInfo();
  static CustomEvent UpdateSongInfo(const model::Song& info);
  static CustomEvent UpdateSongState(const model::Song::CurrentInformation& new_state);
  static CustomEvent DrawAudioRaw(int* buffer, int buffer_size);

  //! Possible events (from interface to audio thread)
  static CustomEvent NotifyFileSelection(const std::filesystem::path file_path);
  static CustomEvent PauseOrResumeSong();
  static CustomEvent ClearCurrentSong();
  
  //! Generic getter for event content
  template <typename T>
  T GetContent() const {
    if (std::holds_alternative<T>(content)) {
      return std::get<T>(content);
    } else {
      return T();
    }
  }

  //! Possible types for content
  using Content =
      std::variant<model::Song, model::Song::CurrentInformation, std::filesystem::path, std::vector<int>>;

  //! Variables
  // P.S. removed private keyword, otherwise wouldn't be possible to use C++ brace initialization
  Type type;        //!< Event group type
  Identifier id;    //!< Unique type identifier for Event
  Content content;  //!< Wrapper for content
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_
