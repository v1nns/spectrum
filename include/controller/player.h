/**
 * \file
 * \brief  Class for player
 */

#ifndef INCLUDE_CONTROLLER_PLAYER_H_
#define INCLUDE_CONTROLLER_PLAYER_H_

#include <filesystem>
#include <memory>

#include "error_table.h"
#include "model/song.h"
#include "view/base/action_listener.h"
#include "view/base/event_dispatcher.h"

namespace controller {

/**
 * @brief TODO: Player controller
 */
class Player : public interface::ActionListener {
 public:
  /**
   * @brief Construct a new Player object
   */
  Player(const std::shared_ptr<interface::EventDispatcher>& d);

  /**
   * @brief Destroy the Player object
   */
  virtual ~Player() = default;

  /* ******************************************************************************************** */
  /**
   * @brief Receive a notification from view that a file has been selected. In other words, user may
   * want to play a music.
   * @param file Complete filepath to file entry selected in block
   */
  void NotifyFileSelection(const std::filesystem::path& file) override;

  /* ******************************************************************************************** */
 private:
  //! Auxiliary method to check if file extension is supported
  bool IsExtensionSupported(const std::filesystem::path& file);

  //! Parse a file into .
  error::Value Load(const std::filesystem::path& file);

  /* ******************************************************************************************** */
 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  std::unique_ptr<Song> curr_song_;                       //!< Current song playing
};

}  // namespace controller
#endif  // INCLUDE_CONTROLLER_PLAYER_H_