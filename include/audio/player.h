/**
 * \file
 * \brief  Class for Audio Player
 */

#ifndef INCLUDE_AUDIO_PLAYER_H_
#define INCLUDE_AUDIO_PLAYER_H_

#include <atomic>
#include <memory>
#include <thread>

#include "driver/alsa.h"  // TODO: replace by generic interface
#include "model/application_error.h"
#include "model/global_resource.h"

namespace audio {

/**
 * @brief Responsible to get audio data and send it to hardware
 */
class Player {
 private:
  /**
   * @brief Construct a new Player object
   */
  Player();

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return Player object
   * @param shared Global data used by the whole application
   * @return std::shared_ptr<Player> Player instance
   */
  static std::unique_ptr<Player> Create(std::shared_ptr<model::GlobalResource> shared);

  /**
   * @brief Destroy the Player object
   */
  virtual ~Player();

  //! Remove these
  Player(const Player& other) = delete;             // copy constructor
  Player(Player&& other) = delete;                  // move constructor
  Player& operator=(const Player& other) = delete;  // copy assignment
  Player& operator=(Player&& other) = delete;       // move assignment

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Initialize internal components for Terminal object
   */
  void Init(std::shared_ptr<model::GlobalResource> shared);

  void AudioHandler();

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Force application to exit
   */
  void Exit();

  /* ******************************************************************************************** */
 private:
  std::unique_ptr<driver::Alsa> driver_;  //!< Interface between spectrum and ALSA
  std::shared_ptr<model::GlobalResource> shared_data_;

  std::thread audio_loop_;  //!< TODO:...
};

}  // namespace audio
#endif  // INCLUDE_AUDIO_PLAYER_H_