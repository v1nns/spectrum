/**
 * \file
 * \brief  Interface class to listen for actions from blocks
 */

#ifndef INCLUDE_VIEW_BASE_LISTENER_H_
#define INCLUDE_VIEW_BASE_LISTENER_H_

#include <filesystem>

#include "model/volume.h"

namespace interface {

/**
 * @brief Interface class to receive an interface action
 */
class Listener {
 public:
  /**
   * @brief Construct a new Action Listener object
   */
  Listener() = default;

  /**
   * @brief Destroy the Action Listener object
   */
  virtual ~Listener() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Notify Audio thread about file selected by user on Terminal User Interface (TUI)
   * @param file Full path to file (may be a song or not)
   */
  virtual void NotifyFileSelection(const std::filesystem::path& file) = 0;

  /**
   * @brief Notify Audio thread to stop playing the current song
   */
  virtual void ClearCurrentSong() = 0;

  /**
   * @brief Notify Audio thread to pause/resume the current song
   */
  virtual void PauseOrResume() = 0;

  /**
   * @brief Notify Audio thread to stop the current song
   */
  virtual void Stop() = 0;

  /**
   * @brief Notify Audio Player to set volume
   * @param value Sound volume information
   */
  virtual void SetVolume(model::Volume value) = 0;

  /**
   * @brief Notify Audio Player to resize quantity of frequency bars as result from audio analysis
   * @param value Maximum quantity of frequency bars
   */
  virtual void ResizeAnalysisOutput(int value) = 0;
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_LISTENER_H_
