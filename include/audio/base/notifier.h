/**
 * \file
 * \brief  Interface class for sending actions from GUI to Audio Player
 */

#ifndef INCLUDE_AUDIO_BASE_NOTIFIER_H_
#define INCLUDE_AUDIO_BASE_NOTIFIER_H_

#include <filesystem>

#include "model/audio_filter.h"
#include "model/playlist.h"
#include "model/volume.h"

namespace audio {

/**
 * @brief Interface class to notify an action to Audio Player
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
   * @brief Notify Audio Player about file selected by user on Terminal User Interface (TUI)
   * @param file Full path to file (may be a song or not)
   */
  virtual void NotifyFileSelection(const std::filesystem::path& file) = 0;

  /**
   * @brief Notify Audio Player to pause the current song
   */
  virtual void Pause() = 0;

  /**
   * @brief Notify Audio Player to resume the current song
   * @param run_animation Flag to execute regain animation before resuming song on audio player
   */
  virtual void Resume(bool run_animation) = 0;

  /**
   * @brief Notify Audio Player to stop the current song
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

  /**
   * @brief Notify Audio Player to seek forward position in current playing song
   * @param value Offset value
   */
  virtual void SeekForwardPosition(int value) = 0;

  /**
   * @brief Notify Audio Player to seek backward position in current playing song
   * @param value Offset value
   */
  virtual void SeekBackwardPosition(int value) = 0;

  /**
   * @brief Notify Audio Player to apply audio filters in the audio chain
   * @param frequencies Vector of audio filters
   */
  virtual void ApplyAudioFilters(const model::EqualizerPreset& filters) = 0;

  /**
   * @brief Notify Audio Player about playlist selected by user
   * @param playlist Song queue
   */
  virtual void NotifyPlaylistSelection(const model::Playlist& playlist) = 0;
};

}  // namespace audio
#endif  // INCLUDE_AUDIO_BASE_NOTIFIER_H_
