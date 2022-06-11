/**
 * \file
 * \brief  Class for Audio Player
 */

#ifndef INCLUDE_AUDIO_PLAYER_H_
#define INCLUDE_AUDIO_PLAYER_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "driver/alsa.h"
#include "driver/decoder.h"
#include "model/application_error.h"
#include "model/song.h"

//! Forward declaration
namespace interface {
class InterfaceNotifier;
}

namespace audio {

/**
 * @brief Interface to control Audio Player
 */
class PlayerControl {
 public:
  virtual void Play(const std::string& filepath) = 0;
  virtual void PauseOrResume() = 0;
  virtual void Stop() = 0;
  virtual void Exit() = 0;
};

/**
 * @brief Responsible to control media and play it on hardware
 */
class Player : public PlayerControl {
 private:
  /**
   * @brief Construct a new Player object
   */
  Player();

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return Player object
   * @param synchronous 'false' to run in a thread, otherwise 'true' and manually call audio loop
   * @return std::shared_ptr<Player> Player instance
   */
  static std::shared_ptr<Player> Create(bool synchronous = false);

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
  //! Internal operations
 private:
  /**
   * @brief Initialize internal components for Terminal object
   */
  void Init(bool synchronous);

  /**
   * @brief Reset all media controls to default value
   */
  void ResetMediaControl();

  /**
   * @brief Main-loop function to decode input stream and write to playback stream
   */
  void AudioHandler();

  /* ******************************************************************************************** */
  //! Binds and registrations
 public:
  /**
   * @brief
   * @param player Audio player control interface
   */
  void RegisterInterfaceNotifier(const std::shared_ptr<interface::InterfaceNotifier>& notifier);

  /* ******************************************************************************************** */
  //! Media Control
 public:
  /**
   * @brief Inform Audio loop to try to decode this file as a song and send it to playback
   * @param filepath Full path to file
   */
  void Play(const std::string& filepath) override;

  /**
   * @brief Inform Audio loop to pause/resume song
   */
  void PauseOrResume() override;

  /**
   * @brief Inform Audio loop to stop decoding and sending song to playback
   */
  void Stop() override;

  /**
   * @brief Exit from Audio loop
   */
  void Exit() override;

  /* ******************************************************************************************** */
  //! Custom class for blocking actions
 private:
  /**
   * @brief A simple structure for synchronization with external events (currently used in two
   * situations: to block thread while waiting to start playing and also for resuming audio when it
   * is paused)
   */
  struct SynchronizedValue {
    std::mutex mutex;                  //!< Control access for internal resources
    std::condition_variable cond_var;  //!< Conditional variable to block thread
    std::function<bool()> wait_until;  //!< Predicate must be satisfied to unblock thread
    std::atomic<bool> value;           //!< Actual value (in this case, used for Media controls)

    //! Overloaded operators
    explicit operator bool() const { return value.load(); }

    SynchronizedValue& operator=(const bool a) {
      value.store(a);
      return *this;
    }

    /**
     * @brief Notify thread to unblock (when it is blocked by condition variable)
     */
    void Notify() { cond_var.notify_one(); }

    /**
     * @brief Block thread until predicate from function is satisfied (in other words, wait until
     * user interface sends events)
     * @return true when value is set as true, otherwise false
     */
    bool WaitForValue() {
      std::unique_lock<std::mutex> lock(mutex);
      cond_var.wait(lock, wait_until);
      return value.load();
    }
  };

  /* ******************************************************************************************** */
  //! Variables

  std::unique_ptr<driver::Alsa> playback_;  //!< Handle playback stream
  std::thread audio_loop_;                  //!< Thread to execute main-loop function

  SynchronizedValue play_;   // Controls when should play the song
  SynchronizedValue pause_;  // Controls to pause the song when asked by UI

  std::atomic<bool> stop_;  //!< Flag to stop playing song
  std::atomic<bool> exit_;  //!< Flag to force to exit from application (usually, exit from thread)

  std::unique_ptr<model::Song> curr_song_;  //!< Current song playing

  std::weak_ptr<interface::InterfaceNotifier> notifier_;  //!< Send notifications to interface
};

}  // namespace audio
#endif  // INCLUDE_AUDIO_PLAYER_H_