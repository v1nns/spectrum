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
#include <random>
#include <thread>
#include <vector>

#include "audio/base/decoder.h"
#include "audio/base/playback.h"
#include "audio/command.h"
#include "model/application_error.h"
#include "model/audio_filter.h"
#include "model/audio_format.h"
#include "model/playlist.h"
#include "model/repeat_mode.h"
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
  virtual void DequeueNextSong() = 0;
  virtual void SkipToNext() = 0;
  virtual void SkipToPrevious() = 0;
  virtual void SetRepeatMode(model::RepeatMode mode) = 0;
  virtual void SetShuffle(bool enabled) = 0;
  virtual void SetAudioDevice(const std::string& device) = 0;
  virtual model::AudioDevices GetAudioDevices() const = 0;
  virtual void Exit() = 0;
};

/**
 * @brief Responsible to control media and play it on hardware
 */
class Player : public AudioControl {
  static constexpr int kNumberChannels = 2;  //!< Decoded audio is always stereo

  //! Format of audio samples expected by analysis (16-bit stereo samples at 44.1 kHz)
  static constexpr model::AudioFormat kAnalysisFormat{};
  static constexpr int kMaxFailedSongs = 3;  //!< Songs from playlist that may fail in a row

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
   * @param device Name of output device chosen by user (empty to let Playback driver choose it,
   * which is also done when the chosen one cannot be used)
   * @param playback Pass playback to be used within Audio thread (optional)
   * @param decoder Pass decoder to be used within Audio thread (optional)
   * @param fetcher Pass streaming fetcher to be used within Audio thread (optional)
   * @param asynchronous Run Audio Player as a thread (default is true)
   * @return std::shared_ptr<Player> Player instance
   */
  static std::shared_ptr<Player> Create(bool verbose, const std::string& device = "",
                                        audio::Playback* playback = nullptr,
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
   * @param device Name of output device chosen by user (empty to let Playback driver choose it)
   */
  void Init(bool asynchronous, const std::string& device);

  /**
   * @brief Reset all media controls to default value
   * @param result Application error code from internal operation
   * @param error_parsing Flag to indicate if error occurred on file parsing
   */
  void ResetMediaControl(error::Code result, bool error_parsing = false);

  /**
   * @brief Handle an audio command from internal queue
   * @param buffer Audio buffer (to playback, in the format expected by it)
   * @param size Number of samples per channel in audio buffer
   * @param analysis Audio buffer to analysis (same audio, but not affected by volume and always in
   * the format expected by analysis)
   * @param analysis_size Number of samples per channel in audio buffer to analysis
   * @param new_position Latest position in the song (in seconds)
   * @param last_position Last position to control when current position has changed
   * @return True if player should keep playing audio, False if not
   */
  bool HandleCommand(void* buffer, int size, void* analysis, int analysis_size,
                     int64_t& new_position, int& last_position);

  /**
   * @brief Main-loop function to decode input stream and write to playback stream
   */
  void AudioHandler();

  /**
   * @brief After a song finishes, play next song from playlist (when available, based on repeat
   * mode)
   */
  void DequeueNextSongFromPlaylist();

  /**
   * @brief Update order of songs from playlist, in case that shuffle was enabled/disabled since
   * last time (songs already played keep their order, only the next ones are shuffled)
   */
  void ApplyShuffle();

  /**
   * @brief Select song to play based on the given command, updating playlist and its position
   * @param command Play (song or playlist), skip (to next or previous song from playlist), or play
   * next song after current one has finished
   * @return Song to play (or nothing, e.g. when playlist has no next song)
   */
  std::optional<model::Song> SelectSong(const Command& command);

  /**
   * @brief Check if there is a song to skip to (only songs from playlist can be skipped)
   * @param command Skip command (to next or previous song)
   * @return True if command can be executed, False if not
   */
  bool CanSkip(const Command& command);

  /**
   * @brief Replace playback stream by a new one on the given output device, keeping the current one
   * when it is not possible (and notifying interface about it)
   * @param device Name of output device (empty to let Playback driver choose it)
   */
  void ChangeDevice(const std::string& device);

  /**
   * @brief Create playback stream on the given output device and configure its parameters, which
   * may change the format of audio samples expected by it (as it depends on the device)
   * @param device Name of output device (empty to let Playback driver choose it)
   * @return error::Code Application error code
   */
  error::Code CreatePlaybackStream(const std::string& device);

  /**
   * @brief Configure parameters on playback stream using the desired format, and get the format of
   * audio samples expected by it (as output device may not support the desired one)
   * @return error::Code Application error code
   */
  error::Code ConfigurePlayback();

  /**
   * @brief Ask playback to use the format from the given song (configuring it again only when it
   * is not the same one from the last song played), and decoder to create samples in the format
   * expected by playback
   * @param song Song to play (already opened by decoder, which fills its audio information)
   * @return error::Code Application error code
   */
  error::Code ConfigureOutput(const model::Song& song);

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
   * @brief Inform audio loop to dequeue next song from playlist (when available)
   */
  void DequeueNextSong() override;

  /**
   * @brief Inform audio loop to play next song from playlist (ignored on last song)
   */
  void SkipToNext() override;

  /**
   * @brief Inform audio loop to play previous song from playlist (on first song, play it again)
   */
  void SkipToPrevious() override;

  /**
   * @brief Set repeat mode for songs from queue (applied when current song finishes or is skipped)
   * @param mode Repeat mode
   */
  void SetRepeatMode(model::RepeatMode mode) override;

  /**
   * @brief Enable/disable shuffle for next songs from queue
   * @param enabled Shuffle state
   */
  void SetShuffle(bool enabled) override;

  /**
   * @brief Inform Audio loop to change output device (when it is not possible, current one is kept)
   * @param device Name of output device (empty to let Playback driver choose it)
   */
  void SetAudioDevice(const std::string& device) override;

  /**
   * @brief Get output devices available to play songs
   * @return Output devices
   */
  model::AudioDevices GetAudioDevices() const override;

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
      std::unique_lock lock(mutex);

      // Sway with empty queue to clear it
      std::deque<Command> dummy;
      dummy.swap(queue);

      if (state != State::Exit) {
        // Set state to idle
        state = State::Idle;

        // Re-add to queue only new requests to play song or to change output device (as they do
        // not depend on the song that was playing)
        std::copy_if(dummy.begin(), dummy.end(), std::back_inserter(queue), [](const Command& c) {
          return c == Command::Identifier::Play || c == Command::Identifier::SetDevice;
        });
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

  std::unique_ptr<model::Song> curr_song_;  //!< Current song playing
  //! Queue of songs from playlist, order to play them (indexes from playlist) and position of
  //! current song in this order (only used by audio thread)
  std::optional<model::Playlist> curr_playlist_;
  std::vector<std::size_t> order_;
  std::size_t curr_position_ = 0;
  bool shuffled_ = false;  //!< Shuffle state when order was created

  std::atomic<model::RepeatMode> repeat_ = model::RepeatMode::Off;  //!< Repeat mode for queue
  std::atomic<bool> shuffle_ = false;                   //!< Shuffle next songs from queue
  std::mt19937 random_engine_{std::random_device{}()};  //!< Used to shuffle songs

  //! Skip command to handle after current song stops (as media control is reset when song stops)
  std::optional<Command> pending_skip_;

  std::atomic<int> failed_songs_ = 0;  //!< Songs from playlist that failed in a row

  std::weak_ptr<interface::Notifier> notifier_;  //!< Send notifications to interface

  int period_size_;  //!< Period size from Playback driver

  //! Format of audio samples that would be sent to playback stream, if supported by output device
  //! (only used by audio thread). It always asks for the widest sample format (as audio is
  //! processed by decoder with more precision than its source), and for the sample rate from the
  //! last song played (to not convert it)
  model::AudioFormat desired_format_{.sample_format = model::SampleFormat::S32};

  //! Format of audio samples expected by playback stream, which is the one supported by output
  //! device that is the closest to the desired format (only used by audio thread)
  model::AudioFormat format_;

  //! Output device chosen by user (empty to let Playback driver choose it, only used by audio
  //! thread)
  std::string device_;

  //! Output device chosen by user that could not be used on initialization (interface is notified
  //! about it as soon as it is registered)
  std::string failed_device_;

  error::Code playback_error_ = error::kSuccess;  //!< Error while writing samples to playback

  bool finished_;  //!< Flag to control when player should not process any new requisitions

  /* ******************************************************************************************** */
  //! Friend class for testing purpose

#ifdef ENABLE_TESTS
  friend class ::PlayerTest;
#endif
};

}  // namespace audio
#endif  // INCLUDE_AUDIO_PLAYER_H_
