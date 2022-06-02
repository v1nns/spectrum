/**
 * \file
 * \brief  Class for media controller
 */

#ifndef INCLUDE_CONTROLLER_MEDIA_H_
#define INCLUDE_CONTROLLER_MEDIA_H_

#include <memory>

#include "model/application_error.h"
#include "model/song.h"
#include "view/base/action_listener.h"
#include "view/base/event_dispatcher.h"

namespace controller {

/**
 * @brief Receives notifications from user events and take action upon these events, like for
 * example, play/pause the highlighted song
 */
class Media : public interface::ActionListener {
 public:
  /**
   * @brief Construct a new Media object
   */
  Media(const std::shared_ptr<interface::EventDispatcher>& d);

  /**
   * @brief Destroy the Media object
   */
  virtual ~Media() = default;

  /* ******************************************************************************************** */

  /**
   * @brief Receive a notification from view that a file has been selected. In other words, user may
   * want to play a music.
   * @param file Complete filepath to file entry selected in block
   */
  void NotifyFileSelection(const std::filesystem::path& file) override;

  /**
   * @brief Clear any information about the current song (and then notify the UI)
   */
  void ClearCurrentSong() override;

  /* ******************************************************************************************** */
 private:
  /**
   * @brief Try to load given file to song object
   * @param file Complete filepath to file entry
   * @return error::Code Error identification
   */
  error::Code Load(const std::filesystem::path& file);

  // TODO: * @return error::Code Error identification
  error::Code Clear();

  /* ******************************************************************************************** */
 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
};

}  // namespace controller
#endif  // INCLUDE_CONTROLLER_MEDIA_H_