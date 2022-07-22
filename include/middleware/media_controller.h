/**
 * \file
 * \brief  Class for media controller
 */

#ifndef INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_
#define INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_

#include <memory>

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
                           const std::shared_ptr<audio::AudioControl>& player_ctl);

 public:
  /**
   * @brief Factory method: Create, initialize internal components and return MediaController object
   * @param dispatcher Event dispatcher for Interface
   * @param player Interface to Audio player
   * @return std::shared_ptr<MediaController> MediaController instance
   */
  static std::shared_ptr<MediaController> Create(
      const std::shared_ptr<interface::Terminal>& terminal,
      const std::shared_ptr<audio::Player>& player);

  /**
   * @brief Destroy the MediaController object
   */
  virtual ~MediaController() = default;

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
  void NotifySongState(const model::Song::State& state) override;

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
  //! Variables
 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Dispatch events for UI blocks
  std::weak_ptr<audio::AudioControl> player_ctl_;         //!< Send events to control Audio Player

  std::vector<int> buffer_;
};

}  // namespace middleware
#endif  // INCLUDE_MIDDLEWARE_MEDIA_CONTROLLER_H_
