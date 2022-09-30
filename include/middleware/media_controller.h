/**
 * \file
 * \brief  Class for media controller
 */

#ifndef INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_
#define INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_

#include <atomic>
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

  /* ******************************************************************************************** */
  //! Actions received from Player and sent to UI

  /**
   * @brief Inform UI to clear song information
   */
  void ClearSongInformation() override;

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

  /**
   * @brief An structure for data synchronization considering external events (wait for audio data
   * from player to run some frequency analysis)
   */
  struct AnalysisDataSynced {
    std::mutex mutex;                  //!< Control access for internal resources
    std::condition_variable notifier;  //!< Conditional variable to block thread

    std::atomic<bool> exit;  //!< Control flag to exit thread

    std::vector<double> buffer;  //!< Input buffer with raw audio data

    /**
     * @brief Block thread until player sends raw audio data.
     *
     * @return True if thread should keep working, False if not
     */
    bool WaitForInput() {
      std::unique_lock<std::mutex> lock(mutex);
      notifier.wait(lock, [&]() {
        // Simply exit, do not wait for input data
        if (exit) return true;

        // There is input to be analyzed
        if (!buffer.empty()) return true;

        return false;
      });

      return !exit;
    }

    /**
     * @brief Get a slice from raw audio data to run frequency analysis
     *
     * @param size Chunk size
     * @return Vector containing raw audio data
     */
    const std::vector<double> Get(int size) {
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
      notifier.notify_one();
    }

    /**
     * @brief Force to exit from audio analysis thread
     */
    void Exit() {
      exit = true;
      notifier.notify_one();
    }
  };

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Send events to UI blocks
  std::weak_ptr<audio::AudioControl> player_ctl_;         //!< Send events to control Audio Player

  std::unique_ptr<driver::Analyzer> analyzer_;  //!< Run FFTs on audio raw data to get spectrum

  std::thread analysis_loop_;  //!< Execute audio-analysis function as a thread

  AnalysisDataSynced analysis_data_;  //!< Controls the audio data synchronization
};

}  // namespace middleware
#endif  // INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_
