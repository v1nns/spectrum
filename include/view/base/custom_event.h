/**
 * \file
 * \brief Structure for a custom event
 */

#ifndef INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_
#define INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_

#include <filesystem>
#include <variant>
#include <vector>

#include "model/audio_filter.h"
#include "model/bar_animation.h"
#include "model/block_identifier.h"
#include "model/song.h"
#include "model/volume.h"

namespace interface {

/**
 * @brief Interface for custom events to be handled by blocks
 */
struct CustomEvent {
  // Possible group type events (it determines event direction)
  enum class Type {
    FromInterfaceToAudioThread = 40000,
    FromAudioThreadToInterface = 40001,
    FromInterfaceToInterface = 40002,
  };

  //! Identifier for all existing events
  enum class Identifier {
    // Events from audio thread to interface
    // TODO: add a better documentation for each one
    ClearSongInfo = 50000,
    UpdateVolume = 50001,
    UpdateSongInfo = 50002,
    UpdateSongState = 50003,
    DrawAudioSpectrum = 50004,
    // Events from interface to audio thread
    NotifyFileSelection = 60000,
    PauseOrResumeSong = 60001,
    StopSong = 60002,
    ClearCurrentSong = 60003,
    SetAudioVolume = 60004,
    ResizeAnalysis = 60005,
    SeekForwardPosition = 60006,
    SeekBackwardPosition = 60007,
    ApplyAudioFilters = 60008,
    // Events from interface to interface
    Refresh = 70000,
    ChangeBarAnimation = 70001,
    ShowHelper = 70002,
    CalculateNumberOfBars = 70003,
    SetPreviousFocused = 70004,
    SetNextFocused = 70005,
    SetFocused = 70006,
    PlaySong = 70007,
    ToggleFullscreen = 70008,
    UpdateBarWidth = 70009,
    SkipToNextSong = 70010,
    SkipToPreviousSong = 70011,
    Exit = 70012,
  };

  //! Overloaded operators
  bool operator==(const Identifier& other) const { return id == other; }
  bool operator!=(const Identifier& other) const { return !operator==(other); }

  //! Output custom event to ostream
  friend std::ostream& operator<<(std::ostream& out, const CustomEvent::Identifier& id);
  friend std::ostream& operator<<(std::ostream& out, const CustomEvent& e);

  //! Possible events (from audio thread to interface)
  static CustomEvent ClearSongInfo();
  static CustomEvent UpdateVolume(const model::Volume& sound_volume);
  static CustomEvent UpdateSongInfo(const model::Song& info);
  static CustomEvent UpdateSongState(const model::Song::CurrentInformation& new_state);
  static CustomEvent DrawAudioSpectrum(const std::vector<double>& data);

  //! Possible events (from interface to audio thread)
  static CustomEvent NotifyFileSelection(const std::filesystem::path& file_path);
  static CustomEvent PauseOrResumeSong();
  static CustomEvent StopSong();
  static CustomEvent ClearCurrentSong();
  static CustomEvent SetAudioVolume(const model::Volume& sound_volume);
  static CustomEvent ResizeAnalysis(int bars);
  static CustomEvent SeekForwardPosition(int offset);
  static CustomEvent SeekBackwardPosition(int offset);
  static CustomEvent ApplyAudioFilters(const model::EqualizerPreset& filters);

  //! Possible events (from interface to interface)
  static CustomEvent Refresh();
  static CustomEvent ChangeBarAnimation(const model::BarAnimation& animation);
  static CustomEvent ShowHelper();
  static CustomEvent CalculateNumberOfBars(int number);
  static CustomEvent SetPreviousFocused();
  static CustomEvent SetNextFocused();
  static CustomEvent SetFocused(const model::BlockIdentifier& id);
  static CustomEvent PlaySong();
  static CustomEvent ToggleFullscreen();
  static CustomEvent UpdateBarWidth();
  static CustomEvent SkipToNextSong();
  static CustomEvent SkipToPreviousSong();

  static CustomEvent Exit();

  //! Possible types for content
  using Content =
      std::variant<std::monostate, model::Song, model::Volume, model::Song::CurrentInformation,
                   std::filesystem::path, std::vector<double>, int, model::EqualizerPreset,
                   model::BarAnimation, model::BlockIdentifier>;

  //! Getter for event identifier
  Identifier GetId() const { return id; }

  //! Generic getter for event content
  template <typename T>
  T GetContent() const {
    if (std::holds_alternative<T>(content)) {
      return std::get<T>(content);
    } else {
      return T();
    }
  }

  //! Variables
  // P.S. removed private keyword, otherwise wouldn't be possible to use C++ brace initialization
  Type type;        //!< Event group type
  Identifier id;    //!< Unique type identifier for Event
  Content content;  //!< Wrapper for content
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_CUSTOM_EVENT_H_
