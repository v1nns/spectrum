/**
 * \file
 * \brief  Interface class for notify interface with information
 */

#ifndef INCLUDE_VIEW_BASE_NOTIFIER_H_
#define INCLUDE_VIEW_BASE_NOTIFIER_H_

#include "model/application_error.h"
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
   * @brief Notify UI to clear any info about the previously song that was playing
   */
  virtual void ClearSongInformation() = 0;

  /**
   * @brief Notify UI with detailed information from the parsed song
   * @param info Detailed audio information from previously file selected
   */
  virtual void NotifySongInformation(const model::Song& info) = 0;

  /**
   * @brief Notify UI with new state information from current song
   * @param state Updated state information
   */
  virtual void NotifySongState(const model::Song::CurrentInformation& state) = 0;

  /**
   * @brief Send raw audio samples to UI
   * @param buffer Audio samples
   * @param buff_size Sample count
   */
  virtual void SendAudioRaw(int* buffer, int buffer_size) = 0;

  /**
   * @brief Notify UI with error code from some background operation
   * @param code Application error code
   */
  virtual void NotifyError(error::Code code) = 0;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_NOTIFIER_H_
