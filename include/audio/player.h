/**
 * \file
 * \brief  Class for Audio Player
 */

#ifndef INCLUDE_AUDIO_PLAYER_H_
#define INCLUDE_AUDIO_PLAYER_H_

#include <atomic>
#include <memory>
#include <thread>

#include "driver/alsa.h"
#include "driver/decoder.h"
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
   * @param synchronous 'false' to run in a thread, otherwise 'true' and manually call audio loop
   * @return std::shared_ptr<Player> Player instance
   */
  static std::unique_ptr<Player> Create(std::shared_ptr<model::GlobalResource> shared,
                                        bool synchronous = false);

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
  void Init(const std::shared_ptr<model::GlobalResource>& shared, bool synchronous);

  /**
   * @brief Main-loop function to decode input stream and write to playback stream
   */
  void AudioHandler();

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Force application to exit
   */
  void Exit();

  /* ******************************************************************************************** */
 private:
  std::unique_ptr<driver::Alsa> playback_;              //!< Handle playback stream
  std::unique_ptr<driver::Decoder> decoder_;            //!< Decode input stream from file
  std::shared_ptr<model::GlobalResource> shared_data_;  //!< Data shared between threads

  std::thread audio_loop_;  //!< Thread to execute main-loop function
};

}  // namespace audio
#endif  // INCLUDE_AUDIO_PLAYER_H_