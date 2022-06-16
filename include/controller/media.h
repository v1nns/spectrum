/**
 * \file
 * \brief  Class for media controller
 */

#ifndef INCLUDE_CONTROLLER_MEDIA_H_
#define INCLUDE_CONTROLLER_MEDIA_H_

#include <memory>

#include "audio/player.h"
#include "model/application_error.h"
#include "model/song.h"
#include "view/base/action_listener.h"
#include "view/base/event_dispatcher.h"
#include "view/base/interface_notifier.h"

namespace controller {

/**
 * @brief Receives notifications from user events and take action upon these events, like for
 * example, play/pause the highlighted song.
 *
 * It is important to highlight that this class is like a middleware to send/receive stuff between
 * UI and Audio thread. And for that, it was decided to split these functionalities into
 * ActionListener (UI->AudioThread) and InterfaceNotifier (AudioThread->UI).
 */
class Media : public interface::ActionListener, public interface::InterfaceNotifier {
 public:
  /**
   * @brief Construct a new Media object
   * @param dispatcher Event dispatcher
   */
  Media(const std::shared_ptr<interface::EventDispatcher>& dispatcher);

  /**
   * @brief Destroy the Media object
   */
  virtual ~Media() = default;

  /**
   * @brief Register player interface for media control
   * @param player Interface to Audio player
   */
  void RegisterPlayerControl(const std::shared_ptr<audio::PlayerControl>& player);

  /* ******************************************************************************************** */
  //! Actions received from UI and sent to Audio thread

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
   * @brief Clear any information about the current song (and then notify the UI)
   */
  void ClearCurrentSong() override;

  /* ******************************************************************************************** */
  //! Actions received from Player and sent to UI

  /**
   * @brief Inform UI that Audio player loaded song with success by sending its information
   * @param info Detailed information about audio data from the current song
   */
  void NotifySongInformation(const model::Song& info) override;

  /**
   * @brief Inform UI to clear song information
   */
  void ClearSongInformation() override;

  /**
   * @brief Notify UI with error code from some background operation
   * @param code Application error code
   */
  void NotifyError(error::Code code) override;

  /* ******************************************************************************************** */
  //! Variables
 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Dispatch events for UI blocks
  std::weak_ptr<audio::PlayerControl> player_ctl_;        //!< Send events to control Audio Player
};

}  // namespace controller
#endif  // INCLUDE_CONTROLLER_MEDIA_H_