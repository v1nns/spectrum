/**
 * \file
 * \brief  Interface class to listen for actions from blocks
 */

#ifndef INCLUDE_VIEW_BASE_ACTION_LISTENER_H_
#define INCLUDE_VIEW_BASE_ACTION_LISTENER_H_

#include <filesystem>

namespace interface {

/**
 * @brief Interface class to receive an interface action
 */
class ActionListener {
 public:
  /**
   * @brief Construct a new Action Listener object
   */
  ActionListener() = default;

  /**
   * @brief Destroy the Action Listener object
   */
  virtual ~ActionListener() = default;

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
};

}  // namespace interface
#endif  // INCLUDE_VIEW_BASE_ACTION_LISTENER_H_