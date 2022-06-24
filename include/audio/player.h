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

#include "audio/base/decoder.h"
#include "audio/base/playback.h"
#include "driver/alsa.h"
#include "driver/ffmpeg.h"
#include "model/application_error.h"
#include "model/song.h"

//! Forward declaration
namespace interface {
class Notifier;
}

namespace {
class PlayerTest;
}

namespace audio {

/**
 * @brief Interface to control Audio Player
 */
class AudioControl {
 public:
  virtual void Play(const std::string& filepath) = 0;
  virtual void PauseOrResume() = 0;
  virtual void Stop() = 0;
  virtual void Exit() = 0;
};

/**
 * @brief Responsible to control media and play it on hardware
 */
class Player : public AudioControl {
 private:
  /**
   * @brief Construct a new Player object
   * @param playback Pointer to playback interface
   * @param decoder Pointer to decoder interface
   */
  explicit Player(std::unique_ptr<driver::Playback>&& playback,
                  std::unique_ptr<driver::Decoder>&& decoder);

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return Player object
   * @param playback Pass playback to be used within Audio thread (optional)
   * @param decoder Pass decoder to be used within Audio thread (optional)
   * @param asynchronous Run Audio Player as a thread (default is true)
   * @return std::shared_ptr<Player> Player instance
   */
  static std::shared_ptr<Player> Create(driver::Playback* playback = nullptr,
                                        driver::Decoder* decoder = nullptr,
                                        bool asynchronous = true);

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
   * @param asynchronous Run Audio Player as a thread
   */
  void Init(bool asynchronous);

  /**
   * @brief Reset all media controls to default value
   * @param err_code Application error code from internal operation
   */
  void ResetMediaControl(error::Code err_code = error::kSuccess);

  /**
   * @brief Main-loop function to decode input stream and write to playback stream
   */
  void AudioHandler();

  /* ******************************************************************************************** */
  //! Binds and registrations
 public:
  /**
   * @brief Register notifier to send events to interface
   * @param notifier Interface notifier
   */
  void RegisterInterfaceNotifier(const std::shared_ptr<interface::Notifier>& notifier);

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
   * @brief A simple structure for data synchronization considering external events (currently used
   * in two situations: to block thread while waiting to start playing and also for resuming audio
   * when it is paused)
   */
  struct MediaControlSynced {
    std::mutex mutex;                     //!< Control access for internal resources
    std::condition_variable cond_var;     //!< Conditional variable to block thread
    std::atomic<bool> play, pause, stop;  //!< Media control
    std::atomic<bool> exit;               //!< Flag to force to exit from application

    /**
     * @brief Reset media controls
     */
    void Reset() {
      play = false;
      pause = false;
      stop = false;
    }

    /**
     * @brief Notify thread to unblock (when it is blocked by condition variable)
     */
    void Notify() { cond_var.notify_one(); }

    /**
     * @brief Wait until interface notifies to play song
     * @return true Start playing
     * @return false Do nothing
     */
    bool WaitForPlay() {
      WaitForSync([&] { return play || exit; });
      return play.load();
    }

    /**
     * @brief Wait until interface notifies to resume song
     * @return true Resume song
     * @return false Do nothing
     */
    bool WaitForResume() {
      WaitForSync([&] { return !pause || exit; });
      return !pause.load();
    }

   private:
    /**
     * @brief Block thread until predicate from function is satisfied (in other words, wait until
     * user interface sends events)
     * @param wait_until Function with predicates to evaluate when it is possible to release mutex
     */
    void WaitForSync(std::function<bool()> wait_until) {
      std::unique_lock<std::mutex> lock(mutex);
      cond_var.wait(lock, wait_until);
    }
  };

  /* ******************************************************************************************** */
  //! Variables

  std::unique_ptr<driver::Playback> playback_;  //!< Handle playback stream
  std::unique_ptr<driver::Decoder> decoder_;    //!< Open file as input stream and parse samples

  std::thread audio_loop_;  //!< Thread to execute main-loop function

  MediaControlSynced media_control_;  // Controls the media (play, pause/resume and stop)

  std::unique_ptr<model::Song> curr_song_;  //!< Current song playing

  std::weak_ptr<interface::Notifier> notifier_;  //!< Send notifications to interface

  /* ******************************************************************************************** */
  //! Friend class for testing purpose
  friend class ::PlayerTest;
};

}  // namespace audio
#endif  // INCLUDE_AUDIO_PLAYER_H_