/**
 * \file
 * \brief  Class for media controller
 */

#ifndef INCLUDE_CONTROLLER_MEDIA_H_
#define INCLUDE_CONTROLLER_MEDIA_H_

#include <memory>

#include "model/application_error.h"
#include "model/global_resource.h"
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
   * @param dispatcher Event dispatcher
   * @param resource Global data
   */
  Media(const std::shared_ptr<interface::EventDispatcher>& dispatcher,
        const std::shared_ptr<model::GlobalResource>& resource);

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
   * @brief Try to load given file as a song object and save it in global resources
   * @param file Full path to file
   * @param info [out] In case of success, struct is filled with song information
   * @return error::Code Error identification
   */
  error::Code ReadFile(const std::filesystem::path& filepath, model::AudioData& info);

  /**
   * @brief Clear song from global resource and reset media flags
   * @return error::Code Error identification
   */
  error::Code Clear();

  /* ******************************************************************************************** */
 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  std::weak_ptr<model::GlobalResource> shared_data_;      //!< Data shared between threads
};

}  // namespace controller
#endif  // INCLUDE_CONTROLLER_MEDIA_H_