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
#include <queue>
#include <string>
#include <thread>

#include "audio/base/decoder.h"
#include "audio/base/playback.h"
#include "driver/alsa.h"
#include "driver/ffmpeg.h"
#include "model/application_error.h"
#include "model/song.h"
#include "util/logger.h"

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
  virtual void SetAudioVolume(model::Volume value) = 0;
  virtual model::Volume GetAudioVolume() = 0;
  virtual void SeekForwardPosition(int value) = 0;
  virtual void SeekBackwardPosition(int value) = 0;
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
   * @brief Initialize internal components for Player object
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
   * @brief Set Audio Volume on playback
   * @param value Sound volume
   */
  void SetAudioVolume(model::Volume value) override;

  /**
   * @brief Get Audio Volume information from playback
   * @return Sound volume
   */
  model::Volume GetAudioVolume() override;

  /**
   * @brief Inform audio loop to seek forward position on current playing song
   * @param value Offset position
   */
  void SeekForwardPosition(int value) override;

  /**
   * @brief Inform audio loop to seek backward position on current playing song
   * @param value Offset position
   */
  void SeekBackwardPosition(int value) override;

  /**
   * @brief Exit from Audio loop
   */
  void Exit() override;

  /* ******************************************************************************************** */
  //! Custom class for blocking actions
 private:
  /**
   * @brief Commands list (used for internal control)
   */
  enum class Command {
    None = 8000,
    Play = 8001,
    PauseOrResume = 8002,
    Stop = 8003,
    SeekForward = 8004,
    SeekBackward = 8005,
    Exit = 8006,
  };

  /**
   * @brief Audio player states list
   */
  enum class State {
    Idle = 9000,
    Play = 9001,
    Pause = 9002,
    Stop = 9003,
    Exit = 9004,
  };

  //! Output command to ostream
  friend std::ostream& operator<<(std::ostream& out, Command& cmd) {
    switch (cmd) {
      case Command::None:
        out << " None ";
        break;
      case Command::Play:
        out << " Play ";
        break;
      case Command::PauseOrResume:
        out << " PauseOrResume ";
        break;
      case Command::Stop:
        out << " Stop ";
        break;
      case Command::SeekForward:
        out << " SeekForward ";
        break;
      case Command::SeekBackward:
        out << " SeekBackward ";
        break;
      case Command::Exit:
        out << " Exit ";
        break;
    }

    return out;
  }

  /**
   * @brief Translate media control command to media state
   *
   * @param cmd Media control command
   * @return Media state
   */
  static State TranslateCommand(const Command cmd) {
    State st = State::Idle;
    switch (cmd) {
      case Command::Play:
      case Command::SeekForward:
      case Command::SeekBackward:
        st = State::Play;
        break;
      case Command::PauseOrResume:
        st = State::Pause;
        break;
      case Command::Stop:
        st = State::Stop;
        break;
      case Command::Exit:
        st = State::Exit;
        break;
      default:
        break;
    }
    return st;
  }

  /**
   * @brief An structure for data synchronization considering external events (currently used in
   * some situations like: to block thread while waiting to start playing and also for resuming
   * audio when it is paused)
   */
  struct MediaControlSynced {
    std::mutex mutex;                  //!< Control access for internal resources
    std::condition_variable notifier;  //!< Conditional variable to block thread

    std::queue<Command> queue;  //!< Queue with media control commands
    std::atomic<State> state;   //!< Current state

    /**
     * @brief Reset media controls
     */
    void Reset() {
      if (state != State::Exit) state = State::Idle;
      std::queue<Command>().swap(queue);
    }

    /**
     * @brief Push command to media control queue
     * @param cmd Media command
     */
    void Push(const Command cmd) {
      {
        std::unique_lock<std::mutex> lock(mutex);

        // Clear queue in case of exit request
        if (cmd == Command::Exit) {
          std::queue<Command>().swap(queue);
          state = State::Exit;
        }

        queue.push(std::move(cmd));
      }
      notifier.notify_one();
    }

    /**
     * @brief Pop command from media control queue
     * @return Media command
     */
    Command Pop() {
      std::unique_lock<std::mutex> lock(mutex);
      if (queue.empty()) return Command::None;

      auto cmd = queue.front();
      queue.pop();

      return cmd;
    }

    /**
     * @brief Block thread until user interface sends events matching the expected command(s). As
     * this is a blocking operation, when one of the expected commands matches with the one from
     * queue, media control state is updated.
     *
     * @tparam Args Media command
     * @param cmds Command list
     * @return True if thread should keep working, False if not
     */
    template <typename... Args>
    bool WaitFor(Args&&... cmds) {
      LOG("Waiting for commands: {", cmds..., "}");
      std::unique_lock<std::mutex> lock(mutex);
      notifier.wait(lock, [&]() {
        // Simply exit, do not wait for any command
        if (state == State::Exit) return true;

        // Pop commands from queue
        std::vector<Command> expected = {Command::Exit, cmds...};
        while (!queue.empty()) {
          Command current = queue.front();
          queue.pop();

          LOG("Received command:", current);

          // Check if it matches with some command from list
          for (auto cmd : expected) {
            if (current == cmd) {
              // In case of match, update state and clear queue
              state = TranslateCommand(current);
              std::queue<Command>().swap(queue);
              return true;
            }
          }
        }

        // No command in queue or didn't match expect command in list
        return false;
      });

      return state != State::Exit;
    }
  };

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::unique_ptr<driver::Playback> playback_;  //!< Handle playback stream
  std::unique_ptr<driver::Decoder> decoder_;    //!< Open file as input stream and parse samples

  std::thread audio_loop_;  //!< Execute audio-loop function as a thread

  MediaControlSynced media_control_;  // Controls the media (play, pause/resume and stop)

  std::unique_ptr<model::Song> curr_song_;  //!< Current song playing

  std::weak_ptr<interface::Notifier> notifier_;  //!< Send notifications to interface

  int period_size_;  //!< Period size from Playback driver

  /* ******************************************************************************************** */
  //! Friend class for testing purpose
  friend class ::PlayerTest;
};

}  // namespace audio
#endif  // INCLUDE_AUDIO_PLAYER_H_
