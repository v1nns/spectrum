/**
 * \file
 * \brief  Class for player
 */

#ifndef INCLUDE_CONTROLLER_PLAYER_H_
#define INCLUDE_CONTROLLER_PLAYER_H_

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

#include "driver/alsa.h"  // TODO: replace by generic interface
#include "model/application_error.h"
#include "model/song.h"
#include "view/base/action_listener.h"
#include "view/base/event_dispatcher.h"

namespace controller {

/**
 * @brief TODO: Player controller
 */
class Player : public interface::ActionListener {
 public:
  /**
   * @brief Construct a new Player object
   */
  Player(const std::shared_ptr<interface::EventDispatcher>& d);

  /**
   * @brief Destroy the Player object
   */
  virtual ~Player();

  /* ******************************************************************************************** */

  /**
   * @brief Receive a notification from view that a file has been selected. In other words, user may
   * want to play a music.
   * @param file Complete filepath to file entry selected in block
   */
  void NotifyFileSelection(const std::filesystem::path& file) override;

  /**
   * @brief Clear any information about the current song (and then notify the UI)
   */
  void ClearCurrentSong() override;

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Try to load given file to song object
   * @param file Complete filepath to file entry
   * @return error::Code Error identification
   */
  error::Code Load(const std::filesystem::path& file);

  /**
   * @brief Start playing the recently loaded song
   */
  void PlaySong();

  // TODO: implement this
  //  error::Code StopSong();

  // * @return error::Code Error identification
  error::Code Clear();

  /* ******************************************************************************************** */
 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks

  std::mutex mutex_;       //!< TODO:...
  std::atomic_bool stop_;  //!< TODO: ...
                           //   std::thread loop_;       //!< TODO:...

  std::unique_ptr<driver::AlsaSound> driver_;  //!< Interface between spectrum and ALSA
  std::unique_ptr<model::Song> curr_song_;     //!< Current song playing
};

}  // namespace controller
#endif  // INCLUDE_CONTROLLER_PLAYER_H_