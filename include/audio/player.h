/**
 * \file
 * \brief  Class for Audio Player
 */

#ifndef INCLUDE_AUDIO_PLAYER_H_
#define INCLUDE_AUDIO_PLAYER_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "audio/base/decoder.h"
#include "audio/base/playback.h"
#include "audio/command.h"
#include "model/application_error.h"
#include "model/audio_filter.h"
#include "model/playlist.h"
#include "model/song.h"
#include "model/volume.h"
#include "util/logger.h"
#include "web/base/stream_fetcher.h"

//! Forward declaration
namespace interface {
class Notifier;
}

#ifdef ENABLE_TESTS
namespace {
class PlayerTest;
}
#endif

namespace audio {

/**
 * @brief Interface to control Audio Player
 */
class AudioControl {
 public:
  AudioControl() = default;
  virtual ~AudioControl() = default;

  virtual void Play(const std::filesystem::path& filepath) = 0;
  virtual void Play(const model::Playlist& playlist) = 0;
  virtual void PauseOrResume() = 0;
  virtual void Stop() = 0;
  virtual void SetAudioVolume(const model::Volume& value) = 0;
  virtual model::Volume GetAudioVolume() const = 0;
  virtual void SeekForwardPosition(int value) = 0;
  virtual void SeekBackwardPosition(int value) = 0;
  virtual void ApplyAudioFilters(const model::EqualizerPreset& filters) = 0;
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
   * @param fetcher Pointer to fetcher interface
   */
  explicit Player(std::unique_ptr<audio::Playback>&& playback,
                  std::unique_ptr<audio::Decoder>&& decoder,
                  std::unique_ptr<web::StreamFetcher>&& fetcher);

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return Player object
   * @param verbose Enable verbose logging messages
   * @param playback Pass playback to be used within Audio thread (optional)
   * @param decoder Pass decoder to be used within Audio thread (optional)
   * @param fetcher Pass streaming fetcher to be used within Audio thread (optional)
   * @param asynchronous Run Audio Player as a thread (default is true)
   * @return std::shared_ptr<Player> Player instance
   */
  static std::shared_ptr<Player> Create(bool verbose, audio::Playback* playback = nullptr,
                                        audio::Decoder* decoder = nullptr,
                                        web::StreamFetcher* fetcher = nullptr,
                                        bool asynchronous = true);

  /**
   * @brief Destroy the Player object
   */
  ~Player() override;

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
   * @param result Application error code from internal operation
   * @param error_parsing Flag to indicate if error occurred on file parsing
   */
  void ResetMediaControl(error::Code result, bool error_parsing = false);

  /**
   * @brief Handle an audio command from internal queue
   * @param buffer Audio buffer
   * @param size Buffer size
   * @param new_position Latest position in the song (in seconds)
   * @param last_position Last position to control when current position has changed
   * @return True if player should keep playing audio, False if not
   */
  bool HandleCommand(void* buffer, int size, int64_t& new_position, int& last_position);

  /**
   * @brief Main-loop function to decode input stream and write to playback stream
   */
  void AudioHandler();

  /**
   * @brief After a song finishes, check if got a next one to play from playlist
   */
  void CheckForNextSongFromPlaylist();

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
  /**
   * @brief Inform Audio loop to try to decode this file as a song and send it to playback
   * @param filepath Full path to file
   */
  void Play(const std::filesystem::path& filepath) override;

  /**
   * @brief Inform Audio loop to play the given playlist
   * @param playlist Song queue
   */
  void Play(const model::Playlist& playlist) override;

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
  void SetAudioVolume(const model::Volume& value) override;

  /**
   * @brief Get Audio Volume information from playback
   * @return Sound volume
   */
  model::Volume GetAudioVolume() const override;

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
   * @brief Inform audio loop to update audio filters in the filter chain
   * @param frequencies Vector of audio filters
   */
  void ApplyAudioFilters(const model::EqualizerPreset& filters) override;

  /**
   * @brief Exit from Audio loop
   */
  void Exit() final;

  /* ******************************************************************************************** */
  //! Custom class for blocking actions
 private:
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

  /**
   * @brief Translate media control command to media state
   *
   * @param cmd Media control command
   * @return Media state
   */
  static State TranslateCommand(const Command& cmd) {
    State st = State::Idle;
    switch (cmd.GetId()) {
      case Command::Identifier::Play:
      case Command::Identifier::SeekForward:
      case Command::Identifier::SeekBackward:
        st = State::Play;
        break;
      case Command::Identifier::PauseOrResume:
        st = State::Pause;
        break;
      case Command::Identifier::Stop:
        st = State::Stop;
        break;
      case Command::Identifier::Exit:
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

    std::deque<Command> queue;               //!< Queue with media control commands
    std::atomic<State> state = State::Idle;  //!< Current state

    /**
     * @brief Reset media controls
     */
    void Reset() {
      // Copy queue and clear it
      std::deque<Command> dummy;
      dummy.swap(queue);

      if (state != State::Exit) {
        // Set state to idle
        state = State::Idle;

        // Re-add to queue only new requests to play song
        std::copy_if(dummy.begin(), dummy.end(), std::back_inserter(queue),
                     [](const Command& c) { return c == Command::Identifier::Play; });
      }
    }

    /**
     * @brief Push command to media control queue
     * @param cmd Media command
     */
    void Push(const Command& cmd) {
      std::unique_lock lock(mutex);

      // Clear queue in case of exit request
      if (cmd == Command::Identifier::Exit) {
        if (queue.size() == 1 && queue.front() == cmd) {
          // Don't do anything else
          return;
        }

        std::deque<Command>().swap(queue);
        state = State::Exit;
      }

      queue.push_back(cmd);
      notifier.notify_one();
    }

    /**
     * @brief Pop command from media control queue
     * @return Media command
     */
    Command Pop() {
      std::unique_lock lock(mutex);
      if (queue.empty()) return Command::None();

      auto cmd = queue.front();
      queue.pop_front();

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
      std::vector<Command::Identifier> expected = {cmds...};
      LOG("Waiting for commands: ", expected);

      std::unique_lock lock(mutex);
      notifier.wait(lock, [this, expected]() mutable {
        // Simply exit, do not wait for any command
        if (state == State::Exit) return true;

        // Pop commands from queue
        while (!queue.empty()) {
          Command current = queue.front();
          LOG("Received command: ", current);

          if (current == Command::Exit()) {
            // In case of exit, update state
            state = TranslateCommand(current);
            return true;
          }

          // Check if it matches with some command from list
          if (std::find(expected.begin(), expected.end(), current.id) != expected.end()) {
            // Found expected command, now unblock thread
            return true;
          }

          // Pop command from queue
          queue.pop_front();
        }

        // No command in queue or didn't match expect command in list
        return false;
      });

      return state != State::Exit;
    }
  };

  /* ******************************************************************************************** */
  //! Variables
  std::unique_ptr<audio::Playback> playback_;    //!< Handle playback stream
  std::unique_ptr<audio::Decoder> decoder_;      //!< Open file as input stream and parse samples
  std::unique_ptr<web::StreamFetcher> fetcher_;  //!< Fetch streaming information from URLs

  std::thread audio_loop_;  //!< Execute audio-loop function as a thread

  MediaControlSynced media_control_;  // Controls the media (play, pause/resume and stop)

  std::unique_ptr<model::Song> curr_song_;        //!< Current song playing
  std::optional<model::Playlist> curr_playlist_;  //!< Queue of songs (origined from playlist)

  std::weak_ptr<interface::Notifier> notifier_;  //!< Send notifications to interface

  int period_size_;  //!< Period size from Playback driver

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::PlayerTest;
#endif
};

}  // namespace audio
#endif  // INCLUDE_AUDIO_PLAYER_H_
