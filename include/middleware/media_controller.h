/**
 * \file
 * \brief  Class for media controller
 */

#ifndef INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_
#define INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "audio/base/analyzer.h"
#include "audio/player.h"
#include "model/application_error.h"
#include "model/song.h"
#include "view/base/event_dispatcher.h"
#include "view/base/listener.h"
#include "view/base/notifier.h"

//! Forward declaration
namespace interface {
class Terminal;
}

namespace audio {
class Player;
}

namespace middleware {

/**
 * @brief Receives notifications from user events and take action upon these events, like for
 * example, ask for player to play/pause the highlighted song.
 *
 * It is important to highlight that this class is like a middleware to send/receive stuff between
 * UI and Player. And for that, it was decided to split these functionalities into
 * Listener (UI->Player) and Notifier (Player->UI).
 */
class MediaController : public interface::Listener, public interface::Notifier {
  /**
   * @brief Construct a new MediaController object
   * @param dispatcher Event dispatcher for Interface
   * @param player Interface to Audio player
   */
  explicit MediaController(const std::shared_ptr<interface::EventDispatcher>& dispatcher,
                           const std::shared_ptr<audio::AudioControl>& player_ctl,
                           std::unique_ptr<driver::Analyzer>&& analyzer);

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return MediaController object
   * @param dispatcher Event dispatcher for Interface
   * @param player Interface to Audio player
   * @param asynchronous Run Audio Analysis as a thread (default is true)
   * @return std::shared_ptr<MediaController> MediaController instance
   */
  static std::shared_ptr<MediaController> Create(
      const std::shared_ptr<interface::Terminal>& terminal,
      const std::shared_ptr<audio::Player>& player, bool asynchronous = true);

  /**
   * @brief Destroy the MediaController object
   */
  virtual ~MediaController();

  /**
   * @brief Exit from Audio Analysis loop
   */
  void Exit();

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  /**
   * @brief Initialize internal components for MediaController object
   * @param number_bars Maximum number of bars to result from frequency analysis
   * @param asynchronous Run Audio Analysis as a thread
   */
  void Init(int number_bars, bool asynchronous);

  /**
   * @brief Main-loop function to analyze input stream and send result to UI
   */
  void AnalysisHandler();

  /* ******************************************************************************************** */
  //! Actions received from UI and sent to Player

  /**
   * @brief Receive a notification from view that a file has been selected. In other words, user may
   * want to play a music. Notify Audio Player about this.
   * @param file Complete filepath to file entry selected in block
   */
  void NotifyFileSelection(const std::filesystem::path& file) override;

  /**
   * @brief Notify Audio Player to pause/resume current song
   */
  void PauseOrResume() override;

  /**
   * @brief Notify Audio thread to stop the current song
   */
  void Stop() override;

  /**
   * @brief Clear any information about the current song. After this, player will trigger
   * ClearSongInformation method.
   */
  void ClearCurrentSong() override;

  /**
   * @brief Notify Audio Player to set volume
   */
  virtual void SetVolume(model::Volume value) override;

  /**
   * @brief Notify Audio Player to resize quantity of frequency bars as result from audio analysis
   * @param value Maximum quantity of frequency bars
   */
  void ResizeAnalysisOutput(int value) override;

  /**
   * @brief Notify Audio Player to seek forward position in current playing song
   * @param value Offset value
   */
  void SeekForwardPosition(int value) override;

  /**
   * @brief Notify Audio Player to seek backward position in current playing song
   * @param value Offset value
   */
  void SeekBackwardPosition(int value) override;

  /* ******************************************************************************************** */
  //! Actions received from Player and sent to UI

  /**
   * @brief Notify UI to clear any info about the song that was playing previously
   * @param playing Last media state
   */
  void ClearSongInformation(bool playing) override;

  /**
   * @brief Inform UI that Audio player loaded song with success by sending its information
   * @param info Detailed information about audio data from the current song
   */
  void NotifySongInformation(const model::Song& info) override;

  /**
   * @brief Notify UI with new state information from current song
   * @param state Updated state information
   */
  void NotifySongState(const model::Song::CurrentInformation& state) override;

  /**
   * @brief Send raw audio samples to UI
   * @param buffer Audio samples
   * @param buff_size Sample count
   */
  void SendAudioRaw(int* buffer, int buffer_size) override;

  /**
   * @brief Notify UI with error code from some background operation
   * @param code Application error code
   */
  void NotifyError(error::Code code) override;

  /* ******************************************************************************************** */
  //! Audio analysis
 private:
  /**
   * @brief Commands list (used for internal control)
   */
  enum class Command {
    None = 10000,
    Analyze = 10001,
    RunClearAnimationWithRegain = 10002,
    RunClearAnimationWithoutRegain = 10003,
    RunRegainAnimation = 10004,
    Exit = 10005,
  };

  /**
   * @brief An structure for data synchronization considering external events (wait for audio data
   * from player to run some frequency analysis)
   */
  struct AnalysisDataSynced {
    std::mutex mutex;                  //!< Control access for internal resources
    std::condition_variable notifier;  //!< Conditional variable to block thread

    std::queue<Command> queue;  //!< Queue with media control commands

    std::vector<double> buffer;  //!< Input buffer with raw audio data

    /**
     * @brief Get a slice from raw audio data to run frequency analysis
     *
     * @param size Chunk size
     * @return Vector containing raw audio data
     */
    const std::vector<double> GetBuffer(int size) {
      std::unique_lock<std::mutex> lock(mutex);
      if (size > buffer.size()) size = buffer.size();

      std::vector<double>::const_iterator first = buffer.begin();
      std::vector<double>::const_iterator last = buffer.begin() + size;

      std::vector<double> output(first, last);
      buffer.erase(first, last);

      return output;
    }

    /**
     * @brief Append raw audio data sent by Audio Player to internal buffer
     *
     * @param input Array with raw data
     * @param size Array size
     */
    void Append(int* input, int size) {
      std::unique_lock<std::mutex> lock(mutex);
      std::vector<double>::const_iterator end = buffer.end();

      buffer.insert(end, input, input + size);

      queue.push(Command::Analyze);
      notifier.notify_one();
    }

    /**
     * @brief Push command to media controller queue
     * @param cmd Command
     */
    void Push(const Command cmd) {
      {
        std::unique_lock<std::mutex> lock(mutex);

        // Clear queue in case of exit request
        if (cmd == Command::Exit) {
          std::queue<Command>().swap(queue);
        }

        queue.push(std::move(cmd));
      }
      notifier.notify_one();
    }

    /**
     * @brief Pop command from media controller queue
     * @return Command
     */
    Command Pop() {
      std::unique_lock<std::mutex> lock(mutex);
      if (queue.empty()) return Command::None;

      auto cmd = queue.front();
      queue.pop();

      return cmd;
    }

    /**
     * @brief Block thread until player sends an event and media controller translate it into a
     * command.
     *
     * @return True if thread should keep working, False if not
     */
    bool WaitForCommand() {
      std::unique_lock<std::mutex> lock(mutex);
      notifier.wait(lock, [&]() {
        // No command in queue
        if (queue.empty()) return false;

        // Do not run regain animation while it has not received any input data from player
        if (queue.size() == 1 && queue.front() == Command::RunRegainAnimation) return false;

        return true;
      });

      return queue.front() != Command::Exit;
    }

    /**
     * @brief Block thread until player sends a command or reaches timeout
     * @param timeout Timestamp deadline
     *
     * @return True if thread unlocked by command, False if reached timeout
     */
    bool WaitForCommandOrUntil(
        const std::chrono::time_point<std::chrono::system_clock,
                                      std::chrono::duration<long double, std::nano>>& timeout) {
      std::unique_lock<std::mutex> lock(mutex);
      notifier.wait_until(lock, timeout, [&]() {
        // No command in queue
        if (queue.empty()) return false;

        return true;
      });

      return !queue.empty();
    }
  };

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Send events to UI blocks
  std::weak_ptr<audio::AudioControl> player_ctl_;         //!< Send events to control Audio Player

  std::unique_ptr<driver::Analyzer> analyzer_;  //!< Run FFTs on audio raw data to get spectrum

  std::thread analysis_loop_;  //!< Execute audio-analysis function as a thread

  AnalysisDataSynced sync_data_;  //!< Controls the audio data synchronization
};

}  // namespace middleware
#endif  // INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_
