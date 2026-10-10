/**
 * \file
 * \brief  Interface class to notify GUI with information
 */

#ifndef INCLUDE_VIEW_BASE_NOTIFIER_H_
#define INCLUDE_VIEW_BASE_NOTIFIER_H_

#include <cstdint>

#include "model/application_error.h"
#include "model/audio_output.h"
#include "model/song.h"

namespace interface {

/**
 * @brief Interface class to notify interface with updated information
 */
class Notifier {
 public:
  /**
   * @brief Construct a new Notifier object
   */
  Notifier() = default;

  /**
   * @brief Destroy the Notifier object
   */
  virtual ~Notifier() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Notify UI to clear any info about the song that was playing previously
   * @param playing Last media state
   */
  virtual void ClearSongInformation(bool playing) = 0;

  /**
   * @brief Notify UI with detailed information from the parsed song
   * @param info Detailed audio information from previously file selected
   */
  virtual void NotifySongInformation(const model::Song& info) = 0;

  /**
   * @brief Notify UI with new state information from current song
   * @param curr_info Updated state information
   */
  virtual void NotifySongState(const model::Song::CurrentInformation& curr_info) = 0;

  /**
   * @brief Send raw audio samples to UI
   * @param buffer Audio samples (16-bit, interleaved channels)
   * @param size Sample count (considering all channels)
   */
  virtual void SendAudioRaw(const int16_t* buffer, int size) = 0;

  /**
   * @brief Notify UI with error code from some background operation
   * @param code Application error code
   * @param detail What the error refers to, like the song file name (optional, may be empty)
   */
  virtual void NotifyError(error::Code code, const std::string& detail) = 0;

  /**
   * @brief Notify UI with audio output used to play current song (when it starts playing, and also
   * when output device is changed in the meantime)
   * @param output Output device in use and format of audio samples sent to it
   */
  virtual void NotifyAudioOutput(const model::AudioOutput& output) = 0;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_NOTIFIER_H_
