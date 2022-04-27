/**
 * \file
 * \brief  Class for player
 */

#ifndef INCLUDE_CONTROLLER_PLAYER_H_
#define INCLUDE_CONTROLLER_PLAYER_H_

#include <filesystem>
#include <memory>

#include "model/song.h"
#include "view/base/terminal.h"

/**
 * @brief TODO: Listen for Block action, MUST THINK WHERE THIS SHOULD BE LOCATED
 */
class BlockListener {
 public:
  virtual void UserSelectedFile() = 0;
};

/**
 * @brief Player controller
 */
class Player : BlockListener {
 public:
  /**
   * @brief Construct a new Player object
   */
  Player(const std::shared_ptr<interface::Dispatcher>& d);

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
  std::weak_ptr<interface::Dispatcher> dispatcher_;
  std::unique_ptr<Song> curr_song_;
};

#endif  // INCLUDE_CONTROLLER_PLAYER_H_