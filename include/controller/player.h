/**
 * \file
 * \brief  Class for player
 */

#ifndef INCLUDE_CONTROLLER_PLAYER_H_
#define INCLUDE_CONTROLLER_PLAYER_H_

#include <filesystem>
#include <memory>

#include "model/song.h"
#include "view/base/action_listener.h"
#include "view/base/event_dispatcher.h"

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
  //! TODO:
  void UserSelectedFile() override;

  /* ******************************************************************************************** */
 private:
  //! TODO:
  bool IsFormatSupported(const std::filesystem::path& file);

  //! TODO: ...
  int Load(const std::filesystem::path& file);

 private:
  std::weak_ptr<interface::EventDispatcher> dispatcher_;  //!< Dispatch events for other blocks
  std::unique_ptr<Song> curr_song_;                       //! TODO: ...
};

#endif  // INCLUDE_CONTROLLER_PLAYER_H_